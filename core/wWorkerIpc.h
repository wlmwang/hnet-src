
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_WORKER_IPC_H_
#define _W_WORKER_IPC_H_

#include <algorithm>
#include <vector>
#include <sys/epoll.h>

#include "wCore.h"
#include "wLog.h"
#include "wMisc.h"
#include "wThread.h"
#include "wTask.h"
#include "wWorker.h"

class wWorker;
class wWorkerIpc : public wThread
{
	public:
		wWorkerIpc(wWorker *pWorker);

		virtual int PrepareRun();
		virtual int Run();
		
		void Recv();
		int InitEpoll();
		void CleanEpoll();
		int AddToEpoll(wTask* pTask, int iEvents = EPOLLIN, int iOp = EPOLL_CTL_ADD);
        int RemoveEpoll(wTask* pTask);
		
		vector<wTask*>::iterator RemoveTaskPool(wTask *pTask);
		int AddToTaskPool(wTask *pTask);
		void CleanTaskPool();
		int PoolNum() { return mTaskPool.size();}
		
		bool IsRunning() { return mStatus == SERVER_RUNNING; }
		SERVER_STATUS &Status() { return mStatus; }

	protected:
		int mErr;
		wWorker *mWorker {NULL};
		
		SERVER_STATUS mStatus {SERVER_INIT};	//服务器当前状态
		int mEpollFD {FD_UNKNOWN};
		int mTimeout {10};

		//epoll_event
		struct epoll_event mEpollEvent;
		vector<struct epoll_event> mEpollEventPool; //epoll_event已发生事件池（epoll_wait）
		int mTaskCount {0};	//mTcpTaskPool.size();
		
		//task|pool
		wTask *mTask {NULL};		//temp task
		vector<wTask*> mTaskPool;
};

#endif
