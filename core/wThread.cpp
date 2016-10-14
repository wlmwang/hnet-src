
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThread.h"
#include "wMutex.h"

namespace hnet {

void* wThread::ThreadWrapper(void *arg) {
    if (arg == NULL) {
	   return NULL;
    }

    wThread *thread = reinterpret_cast<wThread *>(arg);
    thread->mStatus = thread->PrepareRun();
    if (thread->mStatus.Ok()) {
    	thread->mStatus = thread->Run();
    }

    /*
    thread->mStatus = thread->PrepareRun();
    if (!thread->mStatus.Ok()) {
    	return;
    }
	while (true) {
		// 线程同步控制
		thread->CondBlock();
		thread->mStatus = thread->Run();
		if (thread->mStatus.Ok()) {
			thread->StopThread();
			break;
		}
	}
	*/
    return NULL;
}

wThread::wThread() : mState(kThreadBlocked), mMutex(NULL), mCond(NULL) { }

wThread::~wThread() {
	SAFE_DELETE(mMutex);
	SAFE_DELETE(mCond);
}

wStatus wThread::StartThread(int join) {
	if (pthread_attr_init(&mAttr) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_init() failed", strerror(errno));
	} else if (pthread_attr_setscope(&mAttr, PTHREAD_SCOPE_SYSTEM) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_setscope() PTHREAD_SCOPE_SYSTEM failed", strerror(errno));
	}

	if (join == 1) {
		if (pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_JOINABLE) != 0) {
			return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_setdetachstate() PTHREAD_CREATE_JOINABLE failed", strerror(errno));
		}
		SAFE_NEW(wMutex, mMutex);
		SAFE_NEW(wCond, mCond);
	} else {
		if (pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_DETACHED) == -1) {
			return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_setdetachstate() PTHREAD_CREATE_DETACHED failed", strerror(errno));
		}
	}

	mState = kThreadRunning;
	if (pthread_create(&mTid, &mAttr, &wThread::ThreadWrapper, reinterpret_cast<void *>(this)) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_create() failed", strerror(errno));
	}

	if (pthread_attr_destroy(&mAttr) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_destroy() failed", strerror(errno));
	}
    return mStatus;
}

wStatus wThread::StopThread() {
    mMutex->Lock();
    mState = kThreadStopped;
    mCond->Signal();
    mMutex->Unlock();

    // 等待该线程终止
    if (pthread_join(mTid, NULL) != 0) {
    	return mStatus = wStatus::IOError("wThread::StopThread, pthread_join() failed", strerror(errno));
    }
    return mStatus;
}

wStatus wThread::CancelThread() {
    mMutex->Lock();
    mState = kThreadStopped;
    mCond->Signal();
    mMutex->Unlock();

    if (pthread_cancel(mTid) != 0) {
    	return mStatus = wStatus::IOError("wThread::CancelThread, pthread_cancel() failed", strerror(errno));
    }
    return mStatus;
}

int wThread::CondBlock() {
    mMutex->Lock();
    // 线程被阻塞或者停止
    while (IsBlocked() || mState == kThreadStopped) {
        if (mState == kThreadStopped) {
            pthread_exit(NULL);
        	return -1;
        }
        // 进入休眠状态
        mState = kThreadBlocked;
        mCond->Wait(*mMutex);
    }

    if (mState != kThreadRunning) {
        mCond->Signal();
    }
    mState = kThreadRunning;
    mMutex->Unlock();
    return 0;
}

int wThread::Wakeup() {
    mMutex->Lock();
    if (!IsBlocked() && mState == kThreadBlocked) {
    	// 向线程发出信号以唤醒
        mCond->Signal();
    }
    mMutex->Unlock();
    return 0;
}

}   // namespace hnet

