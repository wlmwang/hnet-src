
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_WORKER_IPC_H_
#define _W_WORKER_IPC_H_

#include <vector>
#include "wCore.h"
#include "wStatus.h"
#include "wThread.h"

namespace hnet {

class wTask;
class wWorker;
class wChannelSocket;

class wWorkerIpc : public wThread {
public:
	wWorkerIpc(wWorker *worker);
	~wWorkerIpc();

	wStatus PrepareRun();
	wStatus Run();
	
	wStatus NewChannelTask(wChannelSocket* sock, wWorker* worker, wTask** ptr);

	wStatus Recv();
	wStatus InitEpoll();
	wStatus CleanEpoll();
	wStatus AddToEpoll(wTask* pTask, int iEvents = EPOLLIN, int iOp = EPOLL_CTL_ADD);
    wStatus RemoveEpoll(wTask* pTask);
	
	vector<wTask*>::iterator RemoveTaskPool(wTask *pTask);
	wStatus AddToTaskPool(wTask *pTask);
	wStatus CleanTaskPool();
	
	//bool IsRunning() { return mStatus == SERVER_RUNNING; }
	//SERVER_STATUS &Status() { return mStatus; }

protected:
	wStatus mStatus;
	wWorker *mWorker;
	
	//SERVER_STATUS mStatus {SERVER_INIT};	//服务器当前状态
	int32_t mEpollFD;
	uint64_t mTimeout;

	//epoll_event
	struct epoll_event mEpollEvent;
	vector<struct epoll_event> mEpollEventPool; //epoll_event已发生事件池（epoll_wait）
	size_t mTaskCount;
	
	//task|pool
	wTask *mTask;
	vector<wTask*> mTaskPool;
};

}	// namespace hnet

#endif