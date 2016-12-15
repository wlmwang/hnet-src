
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThread.h"
#include "wMutex.h"

namespace hnet {

void* wThread::ThreadWrapper(void *argv) {
    wThread *thread = reinterpret_cast<wThread *>(argv);

    thread->mMutex->Lock();
	thread->mAlive = true;
	thread->mCond->Broadcast();
	thread->mMutex->Unlock();

	thread->RunThread();

    thread->mMutex->Lock();
	thread->mAlive = false;
	thread->mCond->Broadcast();
	thread->mMutex->Unlock();

	// 如果不是mJoinable，需要回收线程资源
	if (!thread->Joinable()) {
		thread->mMutex->Lock();
		while(thread->mAlive) {
			thread->mCond->Wait(*thread->mMutex);
		}
		thread->mMutex->Unlock();
		SAFE_DELETE(thread);
	}

	pthread_exit(NULL);
}

wThread::wThread(bool join) : mJoinable(join), mAlive(false), mMutex(NULL), mCond(NULL) {
	SAFE_NEW(wMutex, mMutex);
	SAFE_NEW(wCond, mCond);
}

wThread::~wThread() {
	SAFE_DELETE(mMutex);
	SAFE_DELETE(mCond);
}

const wStatus& wThread::StartThread() {
	if (pthread_attr_init(&mAttr) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_init() failed", strerror(errno));
	} else if (pthread_attr_setscope(&mAttr, PTHREAD_SCOPE_SYSTEM) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_setscope() PTHREAD_SCOPE_SYSTEM failed", strerror(errno));
	}

	if (mJoinable) {
		if (pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_JOINABLE) != 0) {
			return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_setdetachstate() PTHREAD_CREATE_JOINABLE failed", strerror(errno));
		}
	} else {
		if (pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_DETACHED) == -1) {
			return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_setdetachstate() PTHREAD_CREATE_DETACHED failed", strerror(errno));
		}
	}

	// 线程预启动入口
	if (!PrepareThread().Ok()) {
		return mStatus;
	}

	if (pthread_create(&mPthreadId, &mAttr, &wThread::ThreadWrapper, reinterpret_cast<void*>(this)) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_create() failed", strerror(errno));
	} else if (pthread_attr_destroy(&mAttr) != 0) {
		return mStatus = wStatus::IOError("wThread::StartThread, pthread_attr_destroy() failed", strerror(errno));
	}

	mMutex->Lock();
	while (!mAlive) {
		mCond->Wait(*mMutex);
	}
	mMutex->Unlock();
    return mStatus.Clear();
}

const wStatus& wThread::JoinThread() {
	if (0 != mPthreadId && mJoinable) {
	    if (pthread_join(mPthreadId, NULL) != 0) {
	    	return mStatus = wStatus::IOError("wThread::StopThread, pthread_join() failed", strerror(errno));
	    }
	    mPthreadId = 0;
		mMutex->Lock();
		while (mAlive) {
			mCond->Wait(*mMutex);
		}
		mMutex->Unlock();
	}
	return mStatus.Clear();
}

}   // namespace hnet

