
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThread.h"

namespace hnet {

void* wThread::ThreadWrapper(void *pvArgs) {
    if (!pvArgs) return NULL;

    wThread *pThread = (wThread *)pvArgs;
    if (pThread->PrepareRun()) pThread->Run();

    return NULL;
}

int wThread::StartThread(int join) {
    pthread_attr_init(&mAttr);
    pthread_attr_setscope(&mAttr, PTHREAD_SCOPE_SYSTEM);	// 设置线程状态为与系统中所有线程一起竞争CPU时间
    if (join == 1) {
        pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_JOINABLE);	// 设置非分离的线程
        mMutex = new wMutex();
        mCond = new wCond();
    } else {
        pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_DETACHED);
    }

    mRunStatus = kThreadRunning;
    pthread_create(&mTid, &mAttr, &wThread::ThreadWrapper, (void *)this);

    pthread_attr_destroy(&mAttr);
    return 0;
}

int wThread::StopThread() {
    mMutex->Lock();
    mRunStatus = kThreadStopped;
    mCond->Signal();
    mMutex->Unlock();

    pthread_join(mTid, NULL);	// 等待该线程终止
    return 0;
}

int wThread::CondBlock() {
    mMutex->Lock();
    while (IsBlocked() || mRunStatus == kThreadStopped) {	// 线程被阻塞或者停止
        if (mRunStatus == kThreadStopped) {
            pthread_exit(NULL);
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

}

