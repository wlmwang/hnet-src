
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_THREAD_H_
#define _W_THREAD_H_

#include <pthread.h>
#include <cstdarg>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wMutex;
class wCond;

class wThread : private wNoncopyable {
public:
    wThread();
    virtual ~wThread();

    virtual wStatus PrepareRun() = 0;
    virtual wStatus Run() = 0;

    wStatus StartThread(int join = 1);
    wStatus StopThread();
    wStatus CancelThread();

    // 唤醒线程
    int Wakeup();
    // 线程同步控制
    int CondBlock();
	
private:
    static void* ThreadWrapper(void *arg);
protected:
    inline bool IsBlocked() {
        return mState == kThreadBlocked;
    }

    inline bool IsRunning() {
        return mState == kThreadRunning;
    }

    inline bool IsStopped() {
        return mState == kThreadStopped;
    }

    enum State_t {
        kThreadInit = 0,
        kThreadBlocked = 1,
        kThreadRunning = 2,
        kThreadStopped = 3
    };

    wStatus mStatus;
    State_t mState;

    wMutex *mMutex;
    wCond *mCond;

    // 线程属性
    pthread_t mTid;
    pthread_attr_t mAttr;
};

}	// namespace hnet

#endif
