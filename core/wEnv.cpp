
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <dirent.h>
#include <pthread.h>
#include <sys/mman.h>
#include <deque>
#include <set>
#include "wEnv.h"
#include "wMisc.h"
#include "wSlice.h"
#include "wMutex.h"
#include "wAtomic.h"

namespace hnet {

namespace {

static inline wStatus IOError(const std::string& context, int errNumber) {
    return wStatus::IOError(context, strerror(errNumber));
}

// 顺序文件访问类
class wPosixSequentialFile : public wSequentialFile {
private:
    std::string mFilename;
    FILE* mFile;

public:
    wPosixSequentialFile(const std::string& fname, FILE* f) : mFilename(fname), mFile(f) { }
    virtual ~wPosixSequentialFile() { fclose(mFile); }

    // 外部保证为同步操作
    virtual wStatus Read(size_t n, wSlice* result, char* scratch) {
        wStatus s;
        size_t r = fread(scratch, 1, n, mFile);
        *result = wSlice(scratch, r);
        if (r < n) {
            if (!feof(mFile)) {
                s = IOError(mFilename, errno);
            }
        }
        return s;
    }

    // 跳过指定字节
    virtual wStatus Skip(uint64_t n) {
        if (fseek(mFile, n, SEEK_CUR)) {
            return IOError(mFilename, errno);
        }
        return wStatus::Nothing();
    }
};

// 随机访问类
class wPosixRandomAccessFile : public wRandomAccessFile {
private:
    std::string mFilename;
    int mFD;

public:
    wPosixRandomAccessFile(const std::string& fname, int fd) : mFilename(fname), mFD(fd) { }
    virtual ~wPosixRandomAccessFile() { close(mFD); }

    virtual wStatus Read(uint64_t offset, size_t n, wSlice* result, char* scratch) const {
        wStatus s;
        ssize_t r = pread(mFD, scratch, n, static_cast<off_t>(offset));
        *result = wSlice(scratch, (r < 0) ? 0 : r);
        if (r < 0) {
            s = IOError(mFilename, errno);
        }
        return s;
    }
};

// 优化存取文件时，可一次性映射到内存中。64-bit说明共享内存足够大（虚址空间）
class wMmapLimiter : private wNoncopyable {
public:
    // 64-bit可创建1000个mmaps，小于64-bit不能创建mmaps
    wMmapLimiter() {
        SetAllowed(sizeof(void*) >= 8 ? 1000 : 0);
    }

    bool Acquire() {
        if (GetAllowed() <= 0) {
            return false;
        }
        wMutexWrapper l(&mMutex);
        intptr_t x = GetAllowed();
        if (x <= 0) {
            return false;
        } else {
            SetAllowed(x - 1);
            return true;
        }
    }

    void Release() {
        wMutexWrapper l(&mMutex);
        SetAllowed(GetAllowed() + 1);
    }

private:
    intptr_t GetAllowed() const {
        return reinterpret_cast<intptr_t>(mAllowed.AcquireLoad());
    }

    // 要求：调用前 mMutex 需加锁
    void SetAllowed(intptr_t v) {
        mAllowed.ReleaseStore(reinterpret_cast<void*>(v));
    }

    wMutex mMutex;
    wAtomic mAllowed;
};

// 基于共享内存随机访问文件类
class wPosixMmapReadableFile: public wRandomAccessFile {
private:
    std::string mFilename;  // 文件名
    void* mMmappedRegion;  // 共享内存起始地址
    size_t mLength;         // 共享内存（文件）长度
    wMmapLimiter* mLimiter; // CPU平台

public:
    // base[0,length-1] contains the mmapped contents of the file.
    wPosixMmapReadableFile(const std::string& fname, void* base, size_t length, wMmapLimiter* limiter) 
    : mFilename(fname), mMmappedRegion(base), mLength(length),mLimiter(limiter) { }

    virtual ~wPosixMmapReadableFile() {
        munmap(mMmappedRegion, mLength);
        mLimiter->Release();
    }

    virtual wStatus Read(uint64_t offset, size_t n, wSlice* result, char* scratch) const {
        wStatus s;
        if (offset + n > mLength) {
            *result = wSlice();
            s = IOError(mFilename, EINVAL);
        } else {
            *result = wSlice(reinterpret_cast<char*>(mMmappedRegion) + offset, n);
        }
        return s;
    }
};

// 写文件类
class wPosixWritableFile : public wWritableFile {
private:
    std::string mFilename;
    FILE* mFile;

public:
    wPosixWritableFile(const std::string& fname, FILE* f) : mFilename(fname), mFile(f) { }

    ~wPosixWritableFile() {
        if (mFile != NULL) {
            fclose(mFile);
        }
    }

    virtual wStatus Append(const wSlice& data) {
        size_t r = fwrite(data.data(), 1, data.size(), mFile);
        if (r != data.size()) {
            return IOError(mFilename, errno);
        }
        return wStatus::Nothing();
    }

    virtual wStatus Close() {
        wStatus result;
        if (fclose(mFile) != 0) {
            result = IOError(mFilename, errno);
        }
        mFile = NULL;
        return result;
    }

    virtual wStatus Flush() {
        if (fflush(mFile) != 0) {
            return IOError(mFilename, errno);
        }
        return wStatus::Nothing();
    }

    // 刷新数据到文件
    virtual wStatus Sync() {
        wStatus result;
        if (fflush(mFile) != 0 || fdatasync(fileno(mFile)) != 0) {
            result = IOError(mFilename, errno);
        }
        return result;
    }
};

// 文件加/解独占锁
static int LockOrUnlock(int fd, bool lock) {
    errno = 0;
    struct flock f;
    memset(&f, 0, sizeof(f));
    f.l_type = (lock ? F_WRLCK : F_UNLCK);  // 写锁（独占锁）
    f.l_whence = SEEK_SET;
    f.l_start = 0;
    f.l_len = 0;
    return fcntl(fd, F_SETLK, &f);
}

// 文件锁句柄
class wPosixFileLock : public wFileLock {
public:
    int mFD;
    std::string mName;
};

// 进程文件锁集合，防止同一个进程多次锁同一文件
class wPosixLockTable {
private:
    wMutex mMutex;
    // 文件锁集合
    std::set<std::string> mLockedFiles;
public:
    bool Insert(const std::string& fname) {
        wMutexWrapper l(&mMutex);
        return mLockedFiles.insert(fname).second;
    }

    void Remove(const std::string& fname) {
        wMutexWrapper l(&mMutex);
        mLockedFiles.erase(fname);
    }
};

class wPosixEnv : public wEnv {
public:
    wPosixEnv();
    virtual ~wPosixEnv() {
        char msg[] = "Destroying wEnv::Default()\n";
        fwrite(msg, 1, sizeof(msg), stderr);
        abort();
    }

    virtual wStatus NewSequentialFile(const std::string& fname, wSequentialFile** result) {
        FILE* f = fopen(fname.c_str(), "r");
        if (f == NULL) {
            *result = NULL;
            return IOError(fname, errno);
        } else {
            SAFE_NEW(wPosixSequentialFile(fname, f), *result);
            return wStatus::Nothing();
        }
    }

    virtual wStatus NewRandomAccessFile(const std::string& fname, wRandomAccessFile** result) {
        *result = NULL;
        wStatus s;
        int fd = open(fname.c_str(), O_RDONLY);
        if (fd < 0) {
            s = IOError(fname, errno);
        } else if (mMmapLimit.Acquire()) { // 是否可创建mmaps
            uint64_t size;
            s = GetFileSize(fname, &size);
            if (s.Ok()) {
                // 创建可读的内存映射，其内存映射的空间进程间共享
                // 对文件写入，相当于输出到文件。实际上直到调用msync()或者munmap()，文件才被更新
                void* base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
                if (base != MAP_FAILED) {
                    SAFE_NEW(wPosixMmapReadableFile(fname, base, size, &mMmapLimit), *result);
                } else {
                    s = IOError(fname, errno);
                }
            }
            close(fd);	// 读取IO，映射后可关闭文件
            if (!s.Ok()) {
                mMmapLimit.Release();
            }
        } else {
            SAFE_NEW(wPosixRandomAccessFile(fname, fd), *result);
        }
        return s;
    }

    virtual wStatus NewWritableFile(const std::string& fname, wWritableFile** result) {
        wStatus s;
        FILE* f = fopen(fname.c_str(), "w");
        if (f == NULL) {
            *result = NULL;
            s = IOError(fname, errno);
        } else {
            SAFE_NEW(wPosixWritableFile(fname, f), *result);
        }
        return s;
    }

    virtual bool FileExists(const std::string& fname) {
        return access(fname.c_str(), F_OK) == 0;
    }

    // 获取某目录下所有文件、目录名
    virtual wStatus GetChildren(const std::string& dir, std::vector<std::string>* result) {
        result->clear();
        DIR* d = opendir(dir.c_str());
        if (d == NULL) {
            return IOError(dir, errno);
        }
        struct dirent* entry;
        while ((entry = readdir(d)) != NULL) {
            result->push_back(entry->d_name);
        }
        closedir(d);
        return wStatus::Nothing();
    }

    virtual wStatus DeleteFile(const std::string& fname) {
        wStatus result;
        if (unlink(fname.c_str()) != 0) {
            result = IOError(fname, errno);
        }
        return result;
    }

    virtual wStatus CreateDir(const std::string& name) {
        wStatus result;
        if (mkdir(name.c_str(), 0755) != 0) {
            result = IOError(name, errno);
        }
        return result;
    }

    virtual wStatus DeleteDir(const std::string& name) {
        wStatus result;
        if (rmdir(name.c_str()) != 0) {
            result = IOError(name, errno);
        }
        return result;
    }

    virtual wStatus GetFileSize(const std::string& fname, uint64_t* size) {
        wStatus s;
        struct stat sbuf;
        if (stat(fname.c_str(), &sbuf) != 0) {
            *size = 0;
            s = IOError(fname, errno);
        } else {
            *size = sbuf.st_size;
        }
        return s;
    }

    virtual wStatus RenameFile(const std::string& src, const std::string& target) {
        wStatus result;
        if (rename(src.c_str(), target.c_str()) != 0) {
            result = IOError(src, errno);
        }
        return result;
    }

    // 创建文件锁
    virtual wStatus LockFile(const std::string& fname, wFileLock** lock) {
        *lock = NULL;
        wStatus result;
        int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd < 0) {
            result = IOError(fname, errno);
        } else if (!mLocks.Insert(fname)) {   // 重复加锁
            close(fd);
            result = wStatus::IOError("lock " + fname, "already held by process");
        } else if (LockOrUnlock(fd, true) == -1) {
            result = IOError("lock " + fname, errno);
            close(fd);
            mLocks.Remove(fname);
        } else {
            wPosixFileLock* my_lock;
            SAFE_NEW(wPosixFileLock(), my_lock);
            my_lock->mFD = fd;
            my_lock->mName = fname;
            *lock = my_lock;
        }
        return result;
    }

    // 释放锁
    virtual wStatus UnlockFile(wFileLock* lock) {
        wPosixFileLock* my_lock = reinterpret_cast<wPosixFileLock*>(lock);
        wStatus result;
        if (LockOrUnlock(my_lock->mFD, false) == -1) {
            result = IOError("unlock", errno);
        }
        mLocks.Remove(my_lock->mName);
        close(my_lock->mFD);
        SAFE_DELETE(my_lock);
        return result;
    }

    // 添加任务到后台任务消费线程中
    virtual void Schedule(void (*function)(void*), void* arg);
    virtual void StartThread(void (*function)(void* arg), void* arg);

    // 获取线程id函数，返回64位类型
    static uint64_t gettid() {
        pthread_t tid = pthread_self();
        uint64_t thread_id = 0;
        memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
        return thread_id;
    }
   
    // 当前微妙时间
    virtual uint64_t NowMicros() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
    }

    // 休眠微妙时间
    virtual void SleepForMicroseconds(int micros) {
        usleep(micros);
    }

private:
    void PthreadCall(const char* label, int result) {
        if (result != 0) {
            fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
            abort();
        }
    }

    // 后台任务线程主函数
    void BGThread();
    // 后台任务线程入口函数，为主函数BGThread的包装器
    static void* BGThreadWrapper(void* arg) {
        reinterpret_cast<wPosixEnv*>(arg)->BGThread();
        return NULL;
    }

    pthread_mutex_t mMutex;
    pthread_cond_t mBgsignal;	// 消费线程条件变量
    pthread_t mBgthread;     // 后台任务消费线程id
    bool mStartedBgthread;	// 是否已开启了后台任务消费线程

    // 线程队列节点结构体
    struct BGItem { void* arg; void (*function)(void*); };
    typedef std::deque<BGItem> BGQueue;
    BGQueue mQueue; // 任务双端链表

    wPosixLockTable mLocks;
    wMmapLimiter mMmapLimit;
};

wPosixEnv::wPosixEnv() : mStartedBgthread(false) {
    PthreadCall("mutex_init", pthread_mutex_init(&mMutex, NULL));
    PthreadCall("cvar_init", pthread_cond_init(&mBgsignal, NULL));
}

void wPosixEnv::Schedule(void (*function)(void*), void* arg) {
    PthreadCall("lock", pthread_mutex_lock(&mMutex));

    // 后台任务线程未开启
    if (!mStartedBgthread) {
        mStartedBgthread = true;
        PthreadCall("create thread", pthread_create(&mBgthread, NULL,  &wPosixEnv::BGThreadWrapper, this));
    }

    // 任务链表为空，唤醒任务消费线程
    if (mQueue.empty()) {
        PthreadCall("signal", pthread_cond_signal(&mBgsignal));
    }

    // 添加任务到队尾
    mQueue.push_back(BGItem());
    mQueue.back().function = function;
    mQueue.back().arg = arg;

    PthreadCall("unlock", pthread_mutex_unlock(&mMutex));
}

// 后台任务线程主函数，是消费任务的守护线程
void wPosixEnv::BGThread() {
    while (true) {
        // 等待任务
        PthreadCall("lock", pthread_mutex_lock(&mMutex));
        while (mQueue.empty()) {
            PthreadCall("wait", pthread_cond_wait(&mBgsignal, &mMutex));
        }

        void (*function)(void*) = mQueue.front().function;
        void* arg = mQueue.front().arg;
        mQueue.pop_front();

        PthreadCall("unlock", pthread_mutex_unlock(&mMutex));
        (*function)(arg);
    }
}

// 启动线程属性
namespace {

struct StartThreadState_t { void (*user_function)(void*);	void* arg;};
}	// namespace

// 线程启动包装器
static void* StartThreadWrapper(void* arg) {
    StartThreadState_t* state = reinterpret_cast<StartThreadState_t*>(arg);
    state->user_function(state->arg);
    SAFE_DELETE(state);
    return NULL;
}

// 启动线程
void wPosixEnv::StartThread(void (*function)(void* arg), void* arg) {
    pthread_t t;
    StartThreadState_t* state;
    SAFE_NEW(StartThreadState_t(), state);
    state->user_function = function;
    state->arg = arg;
    PthreadCall("start thread", pthread_create(&t, NULL,  &StartThreadWrapper, state));
}

// 写字符到文件接口
static wStatus DoWriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname, bool should_sync) {
    wWritableFile* file;
    wStatus s = env->NewWritableFile(fname, &file);
    if (!s.Ok()) {
        return s;
    }
    s = file->Append(data);

    if (s.Ok() && should_sync) {
        s = file->Sync();
    }
    if (s.Ok()) {
        s = file->Close();
    }
    SAFE_DELETE(file);

    if (!s.Ok()) {
        env->DeleteFile(fname);
    }
    return s;
}

}	// namespace anonymous

// 异步写字符到文件
wStatus WriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname) {
    return DoWriteStringToFile(env, data, fname, false);
}

// 同步写入字符到文件
wStatus WriteStringToFileSync(wEnv* env, const wSlice& data, const std::string& fname) {
    return DoWriteStringToFile(env, data, fname, true);
}

// 顺序读文件到data中
wStatus ReadFileToString(wEnv* env, const std::string& fname, std::string* data) {
    data->clear();
    wSequentialFile* file;
    wStatus s = env->NewSequentialFile(fname, &file);
    if (!s.Ok()) {
        return s;
    }
    static const int kBufferSize = 8192;
    char* space;
    SAFE_NEW_VEC(kBufferSize, char, space);
    while (true) {
        wSlice fragment;
        s = file->Read(kBufferSize, &fragment, space);
        if (!s.Ok()) {
            break;
        }
        data->append(fragment.data(), fragment.size());
        if (fragment.empty()) {
            break;
        }
    }
    SAFE_DELETE_VEC(space);
    SAFE_DELETE(file);
    return s;
}

// 实例化env对象
static pthread_once_t once = PTHREAD_ONCE_INIT;
static wEnv* defaultEnv;
static void InitDefaultEnv() {
    SAFE_NEW(wPosixEnv(), defaultEnv);
}

wEnv* wEnv::Default() {
    pthread_once(&once, InitDefaultEnv);
    return defaultEnv;
}

}	// namespace hnet
