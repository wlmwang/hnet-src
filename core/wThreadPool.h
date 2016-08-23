
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_THREAD_POOL_H_
#define _W_THREAD_POOL_H_

#include <algorithm>
#include <vector>

#include "wCore.h"
#include "wAssert.h"
#include "wMutex.h"
#include "wNoncopyable.h"
#include "wThread.h"

class wThreadPool : private wNoncopyable
{
	public:
		wThreadPool() {}
		virtual ~wThreadPool();
		void CleanPool();
		
		void PrepareStart();
		void Start(bool bDaemon = true);
		
		virtual void PrepareRun() {}
		virtual void Run() {}

		virtual int AddPool(wThread* pThread);
		virtual int DelPool(wThread* pThread);
		
	protected:
		vector<wThread *> mThreadPool;
		wMutex *mMutex {NULL};
		wCond *mCond {NULL};
};

#endif
