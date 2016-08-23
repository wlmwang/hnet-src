
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThreadPool.h"

wThreadPool::~wThreadPool()
{
	SAFE_DELETE(mMutex);
	SAFE_DELETE(mCond);
	vector<wThread *>::iterator it = mThreadPool.begin();
	for (it; it != mThreadPool.end(); it++)
	{
		SAFE_DELETE(*it);
	}
	mThreadPool.clear();
}

void wThreadPool::CleanPool()
{
	//
}

int wThreadPool::AddPool(wThread* pThread)
{
	W_ASSERT(pThread != NULL, return -1);

	mThreadPool.push_back(pThread);
	LOG_DEBUG(ELOG_KEY, "[system] tid(%d) add into thread pool", pThread->GetTid());
	return 0;
}

int wThreadPool::DelPool(wThread* pThread)
{
	W_ASSERT(pThread != NULL, return -1);
}

void wThreadPool::PrepareStart()
{
	PrepareRun();
}

void wThreadPool::Start(bool bDaemon = true)
{
	Run();
	vector<wThread *>::iterator it = mThreadPool.begin();
	for (it; it != mThreadPool.end(); it++)
	{
		(*it)->StartThread();
	}
}
