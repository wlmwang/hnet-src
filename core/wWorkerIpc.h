
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

	virtual wStatus PrepareRun();
	virtual wStatus Run();

protected:
    // 事件读写主调函数
    wStatus Recv();

	wStatus InitEpoll();
    wStatus AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool newconn = true);
    wStatus RemoveTask(wTask* task);
    wStatus CleanTask();

    wStatus AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    wStatus CleanTaskPool();

	wStatus mStatus;
	wWorker *mWorker;
	
	int32_t mEpollFD;
	uint64_t mTimeout;
	
	// task|pool
	wTask *mTask;
	std::vector<wTask*> mTaskPool;
};

}	// namespace hnet

#endif