
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ENV_H_
#define _W_ENV_H_

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

    // If another mmap slot is available, acquire it and return true.
    // Else return false.
    bool Acquire() {
        if (GetAllowed() <= 0) {
            return false;
        }
        wMutexWrapper l(&mu_);
        intptr_t x = GetAllowed();
        if (x <= 0) {
            return false;
        } else {
            SetAllowed(x - 1);
            return true;
        }
    }

    // Release a slot acquired by a previous call to Acquire() that returned true.
    void Release() {
        wMutexWrapper l(&mu_);
        SetAllowed(GetAllowed() + 1);
    }

private:
    wMutex mu_;
    wAtomic allowed_;

    intptr_t GetAllowed() const {
        return reinterpret_cast<intptr_t>(allowed_.Acquire_Load());
    }

    // 要求：调用前 mu_ 需加锁
    void SetAllowed(intptr_t v) {
        allowed_.Release_Store(reinterpret_cast<void*>(v));
    }
};

// 基于共享内存随机访问文件类
class wPosixMmapReadableFile: public wRandomAccessFile {
private:
    std::string filename_;  // 文件名
    void* mmapped_region_;  // 共享内存起始地址
    size_t length_;         // 共享内存（文件）长度
    wMmapLimiter* limiter_; // CPU平台

public:
    // base[0,length-1] contains the mmapped contents of the file.
    wPosixMmapReadableFile(const std::string& fname, void* base, size_t length, wMmapLimiter* limiter) 
    : filename_(fname), mmapped_region_(base), length_(length),limiter_(limiter) { }

    virtual ~wPosixMmapReadableFile() {
        munmap(mmapped_region_, length_);
        limiter_->Release();
    }

    virtual wStatus Read(uint64_t offset, size_t n, wSlice* result, char* scratch) const {
        wStatus s;
        if (offset + n > length_) {
            *result = wSlice();
            s = IOError(filename_, EINVAL);
        } else {
            *result = wSlice(reinterpret_cast<char*>(mmapped_region_) + offset, n);
        }
        return s;
    }
};

// 写文件类
class wPosixWritableFile : public wWritableFile {
private:
    std::string filename_;
    FILE* file_;

public:
    wPosixWritableFile(const std::string& fname, FILE* f) : filename_(fname), file_(f) { }

    ~wPosixWritableFile() {
        if (file_ != NULL) {
            fclose(file_);
        }
    }

    virtual wStatus Append(const wSlice& data) {
        size_t r = fwrite(data.data(), 1, data.size(), file_);
        if (r != data.size()) {
            return IOError(filename_, errno);
        }
        return wStatus::Nothing();
    }

    virtual wStatus Close() {
        wStatus result;
        if (fclose(file_) != 0) {
            result = IOError(filename_, errno);
        }
        file_ = NULL;
        return result;
    }

    virtual wStatus Flush() {
        if (fflush(file_) != 0) {
            return IOError(filename_, errno);
        }
        return wStatus::Nothing();
    }

    // 刷新数据到文件
    virtual wStatus Sync() {
        wStatus result;
        if (fflush(file_) != 0 || fdatasync(fileno(file_)) != 0) {
            result = IOError(filename_, errno);
        }
        return s;
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
    int fd_;
    std::string name_;
};

// 进程文件锁集合，防止同一个进程多次锁同一文件
class wPosixLockTable {
private:
    wMutex mu_;
    std::set<std::string> locked_files_;  // 文件锁集合
public:
    bool Insert(const std::string& fname) {
        wMutexWrapper l(&mu_);
        return locked_files_.insert(fname).second;
    }

    void Remove(const std::string& fname) {
        wMutexWrapper l(&mu_);
        locked_files_.erase(fname);
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
            *result = new wPosixSequentialFile(fname, f);
            return wStatus::Nothing();
        }
    }

    virtual wStatus NewRandomAccessFile(const std::string& fname, wRandomAccessFile** result) {
        *result = NULL;
        wStatus s;
        int fd = open(fname.c_str(), O_RDONLY);
        if (fd < 0) {
            s = IOError(fname, errno);
        } else if (mmap_limit_.Acquire()) { // 是否可创建mmaps
            uint64_t size;
            s = GetFileSize(fname, &size);
            if (s.Ok()) {
                // 创建可读的内存映射，其内存映射的空间进程间共享
                // 对文件写入，相当于输出到文件。实际上直到调用msync()或者munmap()，文件才被更新
                void* base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
                if (base != MAP_FAILED) {
                    *result = new wPosixMmapReadableFile(fname, base, size, &mmap_limit_);
                } else {
                    s = IOError(fname, errno);
                }
            }
            close(fd);	// 读取IO，映射后可关闭文件
            if (!s.Ok()) {
                mmap_limit_.Release();
            }
        } else {
            *result = new wPosixRandomAccessFile(fname, fd);
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
            *result = new wPosixWritableFile(fname, f);
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

    virtual Status DeleteFile(const std::string& fname) {
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
        } else if (!locks_.Insert(fname)) {   // 重复加锁
            close(fd);
            result = wStatus::IOError("lock " + fname, "already held by process");
        } else if (LockOrUnlock(fd, true) == -1) {
            result = IOError("lock " + fname, errno);
            close(fd);
            locks_.Remove(fname);
        } else {
            wPosixFileLock* my_lock = new wPosixFileLock;
            my_lock->fd_ = fd;
            my_lock->name_ = fname;
            *lock = my_lock;
        }
        return result;
    }

    // 释放锁
    virtual wStatus UnlockFile(wFileLock* lock) {
        wPosixFileLock* my_lock = reinterpret_cast<wPosixFileLock*>(lock);
        wStatus result;
        if (LockOrUnlock(my_lock->fd_, false) == -1) {
            result = IOError("unlock", errno);
        }
        locks_.Remove(my_lock->name_);
        close(my_lock->fd_);
        delete my_lock;
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

    virtual wStatus NewLogger(const std::string& fname, wLogger** result) {
        FILE* f = fopen(fname.c_str(), "w");
        if (f == NULL) {
            *result = NULL;
            return IOError(fname, errno);
        } else {
            *result = new wPosixLogger(f, &wPosixEnv::gettid);
            return Status::Nothing();
        }
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

    // BGThread() is the body of the background thread
    // 后台任务线程主函数
    void BGThread();
    // 后台任务线程入口函数，为主函数BGThread的包装器
    static void* BGThreadWrapper(void* arg) {
        reinterpret_cast<PosixEnv*>(arg)->BGThread();
        return NULL;
    }

    pthread_mutex_t mu_;
    pthread_cond_t bgsignal_;	// 消费线程条件变量
    pthread_t bgthread_;     // 后台任务消费线程id
    bool started_bgthread_;	// 是否已开启了后台任务消费线程

    // Entry per Schedule() call
    // 线程队列节点结构体
    struct BGItem { void* arg; void (*function)(void*); };
    typedef std::deque<BGItem> BGQueue;
    BGQueue queue_; // 任务双端链表

    wPosixLockTable locks_;
    wMmapLimiter mmap_limit_;
};

wPosixEnv::wPosixEnv() : started_bgthread_(false) {
    PthreadCall("mutex_init", pthread_mutex_init(&mu_, NULL));
    PthreadCall("cvar_init", pthread_cond_init(&bgsignal_, NULL));
}

void wPosixEnv::Schedule(void (*function)(void*), void* arg) {
    PthreadCall("lock", pthread_mutex_lock(&mu_));

    // Start background thread if necessary
    if (!started_bgthread_) {   // 后台任务线程未开启
        started_bgthread_ = true;
        PthreadCall(
        "create thread",
        pthread_create(&bgthread_, NULL,  &PosixEnv::BGThreadWrapper, this));
    }

    // If the queue is currently empty, the background thread may currently be
    // waiting.
    if (queue_.empty()) {
        PthreadCall("signal", pthread_cond_signal(&bgsignal_));	// 任务链表为空，唤醒任务消费线程   ???竞争条件???
    }

    // 添加任务到队尾
    queue_.push_back(BGItem());
    queue_.back().function = function;
    queue_.back().arg = arg;

    PthreadCall("unlock", pthread_mutex_unlock(&mu_));
}

// 后台任务线程主函数，是消费任务的守护线程
void wPosixEnv::BGThread() {
    while (true) {
        // Wait until there is an item that is ready to run
        PthreadCall("lock", pthread_mutex_lock(&mu_));
        while (queue_.empty()) {
            PthreadCall("wait", pthread_cond_wait(&bgsignal_, &mu_));
        }

        void (*function)(void*) = queue_.front().function;
        void* arg = queue_.front().arg;
        queue_.pop_front();

        PthreadCall("unlock", pthread_mutex_unlock(&mu_));
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
    delete state;
    return NULL;
}

// 启动线程
void wPosixEnv::StartThread(void (*function)(void* arg), void* arg) {
    pthread_t t;
    StartThreadState_t* state = new StartThreadState_t();
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
    delete file;

    if (!s.Ok()) {
        env->DeleteFile(fname);
    }
    return s;
}

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
    char* space = new char[kBufferSize];
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
    delete[] space;
    delete file;
    return s;
}

}	// namespace anonymous


// 实例化env对象
static pthread_once_t once = PTHREAD_ONCE_INIT;
static wEnv* defaultEnv;
static void InitDefaultEnv() { defaultEnv = new wPosixEnv();}

wEnv* wEnv::Default() {
    pthread_once(&once, InitDefaultEnv);
    return defaultEnv;
}

}	// namespace hnet