
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThread.h"

/**
 *  线程入口
 */
void* ThreadProc(void *pvArgs)
{
	if(!pvArgs)
	{
		return NULL;
	}

	wThread *pThread = (wThread *)pvArgs;

	if(pThread->PrepareRun())
	{
		return NULL;
	}

	pThread->Run();
	return NULL;	//pthread_exit(0);
}

int wThread::StartThread(int join)
{
	pthread_attr_init(&mAttr);
	//设置线程状态为与系统中所有线程一起竞争CPU时间
	pthread_attr_setscope(&mAttr, PTHREAD_SCOPE_SYSTEM);
	if(join == 1)
	{
		pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_JOINABLE);	//设置非分离的线程
		mMutex = new wMutex();
		mCond = new wCond();
	}
	else
	{
		pthread_attr_setdetachstate(&mAttr, PTHREAD_CREATE_DETACHED);	//设置分离的线程
	}
	
	mRunStatus = THREAD_RUNNING;
	pthread_create(&mTid, &mAttr, ThreadProc, (void *)this);
	
	pthread_attr_destroy(&mAttr);
	return 0;
}

int wThread::StopThread()
{
	mMutex->Lock();

	mRunStatus = THREAD_STOPPED;
	mCond->Signal();

	mMutex->Unlock();
	
	//等待该线程终止
	pthread_join(mTid, NULL);

	return 0;
}

int wThread::CondBlock()
{
	mMutex->Lock();		//互斥锁

	while(IsBlocked() || mRunStatus == THREAD_STOPPED)  //线程被阻塞或者停止
	{
		if(mRunStatus == THREAD_STOPPED)  //如果线程需要停止则终止线程
		{
			pthread_exit((void *)GetRetVal());
		}
		
		mRunStatus = THREAD_BLOCKED;	//"blocked"
		
		mCond->Wait(*mMutex);	//进入休眠状态
	}

	if(mRunStatus != THREAD_RUNNING)  
	{
		//"Thread waked up"
	}
	
	mRunStatus = THREAD_RUNNING;  //线程状态变为THREAD_RUNNING

	mMutex->Unlock();	//该过程需要在线程锁内完成
	return 0;
}

int wThread::Wakeup()
{
	mMutex->Lock();

	if(!IsBlocked() && mRunStatus == THREAD_BLOCKED)
    {
		mCond->Signal();	//向线程发出信号以唤醒
	}

	mMutex->Unlock();
	return 0;
}

inline int wThread::CancelThread()
{
	return pthread_cancel(mTid);
}

inline char* wThread::GetRetVal()
{
	memcpy(mRetVal, "pthread exited", sizeof("pthread exited"));
	return mRetVal;
}