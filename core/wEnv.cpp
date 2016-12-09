
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <deque>
#include <vector>
#include <algorithm>
#include "wEnv.h"
#include "wFile.h"
#include "wLogger.h"
#include "wSem.h"
#include "wMutex.h"

namespace hnet {

namespace {

// 平台实现类
class wPosixEnv : public wEnv {
public:
    wPosixEnv();
    virtual ~wPosixEnv() { }

    virtual const wStatus& NewSequentialFile(const std::string& fname, wSequentialFile** result) {
        FILE* f = fopen(fname.c_str(), "r");
        if (f == NULL) {
            *result = NULL;
            return mStatus = wStatus::IOError(fname, strerror(errno));
        } else {
            SAFE_NEW(wPosixSequentialFile(fname, f), *result);
        }
        return mStatus.Clear();
    }

    virtual const wStatus& NewRandomAccessFile(const std::string& fname, wRandomAccessFile** result) {
        *result = NULL;
        int fd = open(fname.c_str(), O_RDONLY);
        if (fd < 0) {
            return mStatus = wStatus::IOError(fname, strerror(errno));
        } else if (mMmapLimit.Acquire()) {
        	// 创建mmaps
            uint64_t size;
            if ((mStatus = GetFileSize(fname, &size)).Ok()) {
                // 创建可读的内存映射，其内存映射的空间进程间共享
                // 对文件写入，相当于输出到文件。实际上直到调用msync()或者munmap()，文件才被更新
                void* base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
                if (base != MAP_FAILED) {
                    SAFE_NEW(wPosixMmapReadableFile(fname, base, size, &mMmapLimit), *result);
                } else {
                	mStatus = wStatus::IOError(fname, strerror(errno));
                }
            }
            // 读取IO，映射后可关闭文件
            close(fd);
            if (!mStatus.Ok()) {
                mMmapLimit.Release();
            }
            return mStatus;
        } else {
            SAFE_NEW(wPosixRandomAccessFile(fname, fd), *result);
        }
        return mStatus.Clear();
    }

    virtual const wStatus& NewWritableFile(const std::string& fname, wWritableFile** result) {
        FILE* f = fopen(fname.c_str(), "w");
        if (f == NULL) {
            *result = NULL;
            return mStatus = wStatus::IOError(fname, strerror(errno));
        } else {
            SAFE_NEW(wPosixWritableFile(fname, f), *result);
        }
        return mStatus.Clear();
    }

    virtual const wStatus& NewSem(const std::string* devshm, wSem** result) {
    	SAFE_NEW(wPosixSem(devshm), *result);
    	if (!(mStatus = (*result)->Open()).Ok()) {
    		SAFE_DELETE(*result);
    	}
    	return mStatus;
    }

    // 创建文件锁
    virtual const wStatus& LockFile(const std::string& fname, wFileLock** lock) {
        *lock = NULL;
        int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd < 0) {
            return mStatus = wStatus::IOError(fname, strerror(errno));
        } else if (!mLocks.Insert(fname)) {   // 重复加锁
            close(fd);
            return mStatus = wStatus::IOError("lock " + fname, "already held by process");
        } else if (LockOrUnlock(fd, true) == -1) {
            close(fd);
            mLocks.Remove(fname);
            return mStatus = wStatus::IOError("lock " + fname, strerror(errno));
        } else {
            wPosixFileLock* my_lock;
            SAFE_NEW(wPosixFileLock(), my_lock);
            my_lock->mFD = fd;
            my_lock->mName = fname;
            *lock = my_lock;
        }
        return mStatus.Clear();
    }

    // 释放锁
    virtual const wStatus& UnlockFile(wFileLock* lock) {
        wPosixFileLock* my_lock = reinterpret_cast<wPosixFileLock*>(lock);
        wStatus result;
        if (LockOrUnlock(my_lock->mFD, false) == -1) {
            return mStatus = wStatus::IOError("unlock", strerror(errno));
        }
        mLocks.Remove(my_lock->mName);
        close(my_lock->mFD);
        SAFE_DELETE(my_lock);
        return mStatus.Clear();
    }

    // 日志对象
    virtual const wStatus& NewLogger(const std::string& fname, wLogger** result, off_t maxsize = 32*1024*1024) {
    	SAFE_NEW(wPosixLogger(fname, &wPosixEnv::getpid, maxsize), *result);
		if (*result == NULL) {
			return mStatus = wStatus::IOError(fname, strerror(errno));
		}
		return mStatus.Clear();
    }

    // 获取某目录下所有文件、目录名
    virtual const wStatus& GetChildren(const std::string& dir, std::vector<std::string>* result, bool fullname = true) {
        result->clear();
        DIR* d = opendir(dir.c_str());
        if (d == NULL) {
            return mStatus = wStatus::IOError(dir, strerror(errno));
        }
        struct dirent* entry;
        std::string dname;
        while ((entry = readdir(d)) != NULL) {
        	if (fullname) {
        		result->push_back(dir + "/" + entry->d_name);
        	} else {
        		result->push_back(entry->d_name);
        	}
        }
        closedir(d);
        return mStatus.Clear();
    }

    virtual const wStatus& GetRealPath(const std::string& fname, std::string* result) {
    	char dirPath[256];
        if (realpath(fname.c_str(), dirPath) == NULL) {
        	return mStatus = wStatus::IOError(fname, strerror(errno));
        }
        *result = dirPath;
        return mStatus.Clear();
    }

    virtual const wStatus& DeleteFile(const std::string& fname) {
        if (unlink(fname.c_str()) != 0) {
            return  mStatus = wStatus::IOError(fname, strerror(errno));
        }
        return mStatus.Clear();
    }

    virtual const wStatus& CreateDir(const std::string& name) {
        if (mkdir(name.c_str(), 0755) != 0) {
            return mStatus = wStatus::IOError(name, strerror(errno));
        }
        return mStatus.Clear();
    }

    virtual const wStatus& DeleteDir(const std::string& name) {
        if (rmdir(name.c_str()) != 0) {
        	return mStatus = wStatus::IOError(name, strerror(errno));
        }
        return mStatus.Clear();
    }

    virtual const wStatus& GetFileSize(const std::string& fname, uint64_t* size) {
        struct stat sbuf;
        if (stat(fname.c_str(), &sbuf) != 0) {
            *size = 0;
            return mStatus = wStatus::IOError(fname, strerror(errno));
        } else {
            *size = sbuf.st_size;
        }
        return mStatus.Clear();
    }

    virtual const wStatus& RenameFile(const std::string& src, const std::string& target) {
        if (rename(src.c_str(), target.c_str()) != 0) {
            return mStatus = wStatus::IOError(src, strerror(errno));
        }
        return mStatus.Clear();
    }

    virtual bool FileExists(const std::string& fname) {
        return access(fname.c_str(), F_OK) == 0;
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

    // 获取线程id函数，返回64位类型
    static uint64_t gettid() {
        pthread_t tid = pthread_self();
        uint64_t thread_id = 0;
        memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
        return thread_id;
    }

    // 获取进程id函数，返回64位类型
    static uint64_t getpid() {
        pid_t pid = ::getpid();
        uint64_t pid_id = 0;
        memcpy(&pid_id, &pid, std::min(sizeof(pid_id), sizeof(pid)));
        return pid_id;
    }

    // 添加任务到后台任务消费线程中
    virtual void Schedule(void (*function)(void*), void* arg);
    virtual void StartThread(void (*function)(void* arg), void* arg);

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
    wStatus mStatus;
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
}	// namespace anonymous

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

}	// namespace anonymous

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
