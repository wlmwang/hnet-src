
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThread.h"
#include "wMutex.h"
#include "wLogger.h"

namespace hnet {

void* wThread::ThreadWrapper(void *argv) {
    wThread *thread = reinterpret_cast<wThread*>(argv);

    // 线程启动成功
    thread->mMutex->Lock();
	thread->mAlive = true;
	thread->mCond->Broadcast();
	thread->mMutex->Unlock();

	// 调用用户函数
	int ret = thread->RunThread();

	// 线程结束
	thread->mMutex->Lock();
	thread->mAlive = false;
	thread->mMutex->Unlock();

	pthread_exit(reinterpret_cast<void*>(ret));
}

wThread::wThread(bool join): mJoinable(join), mAlive(false) {
	SAFE_NEW(wMutex, mMutex);
	SAFE_NEW(wCond, mCond);
}

wThread::~wThread() {
	SAFE_DELETE(mMutex);
	SAFE_DELETE(mCond);
}

int wThread::StartThread() {
	// 初始化线程对象的属性
	if (pthread_attr_init(&mAttr) != 0) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread pthread_attr_init() failed", error::Strerror(errno).c_str());
		return -1;
	}

	// 设置线程的作用域
	// PTHREAD_SCOPE_SYSTEM 系统级上竞争资源
	// PTHREAD_SCOPE_PROCESS 进程内竞争资源
	if (pthread_attr_setscope(&mAttr, PTHREAD_SCOPE_SYSTEM) != 0) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread pthread_attr_setscope() failed", error::Strerror(errno).c_str());
		return -1;
	}

	if (mJoinable) {	// 结合线程
		if (pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_JOINABLE) != 0) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread pthread_attr_setdetachstate(PTHREAD_CREATE_JOINABLE) failed", error::Strerror(errno).c_str());
			return -1;
		}
	} else {	// 分离线程
		if (pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_DETACHED) == -1) {			
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread pthread_attr_setdetachstate(PTHREAD_CREATE_DETACHED) failed", error::Strerror(errno).c_str());
			return -1;
		}
	}

	if (PrepareThread() == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread PrepareThread() failed", "");
		return -1;
	}

	// 创建线程
	if (pthread_create(&mPthreadId, &mAttr, &wThread::ThreadWrapper, reinterpret_cast<void*>(this)) != 0) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread pthread_create() failed", error::Strerror(errno).c_str());
		return -1;
	}

	// 等待线程启动完成（注意：不等待函数执行结束）
	// 避免pthread_create返回之前，线程就已终止
	mMutex->Lock();
	while (!mAlive) {
		mCond->Wait(*mMutex);
	}
	mMutex->Unlock();
	
	// 释放属性
	if (pthread_attr_destroy(&mAttr) != 0) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::StartThread pthread_attr_destroy() failed", error::Strerror(errno).c_str());
		return -1;
	}
    return 0;
}

int wThread::JoinThread() {
	if (0 != mPthreadId && mJoinable) {
		// 线程阻塞，直到mPthreadId线程结束返回，被等待线程的资源被收回
	    if (pthread_join(mPthreadId, NULL) != 0) {
	    	if (errno != ESRCH) {
				H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wThread::JoinThread pthread_join() failed", error::Strerror(errno).c_str());
				return -1;
	    	}
	    }
	    mPthreadId = 0;
	}
	return 0;
}

}   // namespace hnet

