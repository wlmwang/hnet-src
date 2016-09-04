
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThread.h"
#include "wLog.h"

namespace hnet {

void* wThread::ThreadWrapper(void *pvArgs) {
    if (!pvArgs) return NULL;

    wThread *pThread = (wThread *)pvArgs;
    if (pThread->PrepareRun()) pThread->Run();

    return NULL;
}

int wThread::StartThread(int join) {
    PthreadCall("attr_init", pthread_attr_init(&mAttr));
    PthreadCall("attr_setscope", pthread_attr_setscope(&mAttr, PTHREAD_SCOPE_SYSTEM));	// 设置线程状态为与系统中所有线程一起竞争CPU时间
    if (join == 1) {
        PthreadCall("attr_setdetachstate", pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_JOINABLE));	// 设置非分离的线程
        mMutex = new wMutex();
        mCond = new wCond();
    } else {
        PthreadCall("attr_setdetachstate", pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_DETACHED));
    }

    mRunStatus = kThreadRunning;
    PthreadCall("create", pthread_create(&mTid, &mAttr, &wThread::ThreadWrapper, (void *)this));

    PthreadCall("attr_destroy", pthread_attr_destroy(&mAttr));
    return 0;
}

int wThread::StopThread() {
    mMutex->Lock();
    mRunStatus = kThreadStopped;
    mCond->Signal();
    mMutex->Unlock();

    PthreadCall("join", pthread_join(mTid, NULL));	// 等待该线程终止
    return 0;
}

int wThread::CondBlock() {
    mMutex->Lock();
    while (IsBlocked() || mRunStatus == kThreadStopped) {	// 线程被阻塞或者停止
        if (mRunStatus == kThreadStopped) {
            PthreadCall("exit", pthread_exit(NULL));
            return -1;
        }
        mRunStatus = kThreadBlocked;
        mCond->Wait(*mMutex);	// 进入休眠状态
    }

    if (mRunStatus != kThreadRunning) {
        // "Thread waked up"
    }

    mRunStatus = kThreadRunning;
    mMutex->Unlock();
    return 0;
}

int wThread::Wakeup() {
    mMutex->Lock();
    if (!IsBlocked() && mRunStatus == kThreadBlocked) {
        mCond->Signal();	// 向线程发出信号以唤醒
    }
    mMutex->Unlock();
    return 0;
}

}   // namespace hnet

