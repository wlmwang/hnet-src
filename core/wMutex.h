
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MUTEX_H_
#define _W_MUTEX_H_

#include <pthread.h>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

static int PthreadCall(const char* label, int errNumber) {
    if (errNumber != 0) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        fprintf(stderr, "pthread %s: %s\n", label, err);
        abort();
    }
    return errNumber;
}

class wCond;

class wMutex : private wNoncopyable {
public:
    // PTHREAD_MUTEX_FAST_NP:  普通锁，同一线程可重复加锁，解锁一次释放锁。不提供死锁检测，尝试重新锁定互斥锁会导致死锁
    // PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP： 同一线程可重复加锁，解锁同样次数才可释放锁
    // PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP： 同一线程不能重复加锁，加上的锁只能由本线程解锁
    // 
    // PTHREAD_PROCESS_PRIVATE： 单进程锁，默认设置
    // PTHREAD_PROCESS_SHARED： 进程间，共享锁（锁变量需进程间共享。如在共享内存中）
	explicit wMutex(int kind = PTHREAD_MUTEX_FAST_NP, int pshared = PTHREAD_PROCESS_PRIVATE) {
        PthreadCall("mutexattr_init", pthread_mutexattr_init(&mAttr));
        PthreadCall("mutexattr_settype", pthread_mutexattr_settype(&mAttr, kind));
        PthreadCall("mutexattr_setpshared", pthread_mutexattr_setpshared(&mAttr, pshared));
        PthreadCall("mutex_init", pthread_mutex_init(&mMutex, &mAttr));
    }

    ~wMutex() {
        PthreadCall("mutexattr_destroy", pthread_mutexattr_destroy(&mAttr));
        PthreadCall("mutex_destroy", pthread_mutex_destroy(&mMutex));
    }

    // 阻塞获取锁
    // 0 成功 EINVAL  锁不合法，mMutex 未被初始化 EDEADLK   重复加锁错误
    inline int Lock() {
        return PthreadCall("mutex_lock", pthread_mutex_lock(&mMutex));
    }

    // 释放锁
    // 0 成功 EPERM 当前线程不是该 mMutex 锁的拥有者
    inline int Unlock() {
        return PthreadCall("mutex_unlock", pthread_mutex_unlock(&mMutex));
    }
    
    // 非阻塞获取锁
    // 0 成功 EBUSY 锁正在使用 EINVAL 锁不合法，mMutex 未被初始化 EAGAIN Mutex的lock count(锁数量)已经超过 递归索的最大值，无法再获得该mutex锁
    inline int TryLock() {
        return pthread_mutex_trylock(&mMutex);
    }

protected:
    pthread_mutex_t mMutex;
    pthread_mutexattr_t mAttr;

    friend class wCond;
};

class wMutexWrapper : private wNoncopyable {
public:
    explicit wMutexWrapper(wMutex* mutex) : mMutex(mutex) {
        mMutex->Lock();
    }

    ~wMutexWrapper() {
        mMutex->Unlock();
    }

private:
    wMutex *mMutex;
};

class wCond : private wNoncopyable {
public:
    // PTHREAD_PROCESS_PRIVATE：单进程条件变量，默认设置
    // PTHREAD_PROCESS_SHARED：进程间，共享条件变量（条件变量需进程间共享。如在共享内存中）
	explicit wCond(int pshared = PTHREAD_PROCESS_PRIVATE) {
        PthreadCall("condattr_init", pthread_condattr_init(&mAttr));
        PthreadCall("condattr_setpshared", pthread_condattr_setpshared(&mAttr, pshared));
        PthreadCall("cond_init", pthread_cond_init(&mCond, &mAttr));
    }

    ~wCond() {
        PthreadCall("condattr_destroy", pthread_condattr_destroy(&mAttr));
        PthreadCall("cond_destroy", pthread_cond_destroy(&mCond));
    }

    inline int Broadcast() {
        return PthreadCall("cond_broadcast", pthread_cond_broadcast(&mCond));
    }

    // 唤醒等待中的线程
    // 使用pthread_cond_signal不会有"惊群现象"产生，他最多只给一个线程发信号
    inline int Signal() {
        return PthreadCall("cond_signal", pthread_cond_signal(&mCond));
    }

    // 阻塞等待特定的条件变量满足
    // stMutex 需要等待的互斥体
    inline int Wait(wMutex &stMutex){
        return PthreadCall("cond_wait", pthread_cond_wait(&mCond, &stMutex.mMutex));
    }

    // 带超时的等待条件
    // stMutex 需要等待的互斥体  tsptr 超时时间
    inline int TimedWait(wMutex &stMutex, struct timespec *tsptr) {
        return pthread_cond_timedwait(&mCond, &stMutex.mMutex, tsptr);
    }

private:
    pthread_cond_t mCond;		//系统条件变量
    pthread_condattr_t mAttr;
};

}   // namespace hnet

#endif
