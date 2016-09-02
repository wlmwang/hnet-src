
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_THREAD_H_
#define _W_THREAD_H_

#include <cstdarg>
#include <pthread.h>

#include "wCore.h"
#include "wLog.h"
#include "wMutex.h"
#include "wNoncopyable.h"

namespace hnet {

static int PthreadCall(const char* label, int result) {
    if (result != 0) {
        fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
        abort();
    }
    return result;
}

class wThread : private wNoncopyable {
public:
    wThread() : mRunStatus(kThreadBlocked), mMutex(NULL), mCond(NULL) { }
    virtual ~wThread() { }

    virtual int PrepareRun() = 0;
    virtual int Run() = 0;

    int StartThread(int join = 1);
    int StopThread();
    int Wakeup();
    int CondBlock();

    pthread_t GetTid() {
        return mTid;
    }
	
private:
    static void* ThreadWrapper(void *pvArgs);

protected:
    int CancelThread() {
        return PthreadCall("pthread_cancle", pthread_cancel(mTid));
    }
    bool IsBlocked() {
        return mRunStatus == kThreadBlocked;
    }
    bool IsRunning() {
        return mRunStatus == kThreadRunning;
    }
    bool IsStopped() {
        return mRunStatus == kThreadStopped;
    }

    enum pthreadStatus {
        kThreadInit = 0,
        kThreadBlocked = 1,
        kThreadRunning = 2,
        kThreadStopped = 3
    };

    pthreadStatus mRunStatus;

    pthread_t mTid;
    pthread_attr_t mAttr;
    wMutex *mMutex;
    wCond *mCond;
};

}	// namespace hnet

#endif
