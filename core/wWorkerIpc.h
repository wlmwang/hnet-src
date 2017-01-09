
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_WORKER_IPC_H_
#define _W_WORKER_IPC_H_

#include <google/protobuf/message.h>
#include <algorithm>
#include <vector>
#include <sys/epoll.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wMutex.h"
#include "wMisc.h"
#include "wSocket.h"
#include "wTimer.h"
#include "wConfig.h"
#include "wThread.h"

namespace hnet {

const int kChannelNumShardBits = 4;
const int kChannelNumShard = 1 << kChannelNumShardBits;

class wWorker;
class wTask;

class wWorkerIpc : public wThread {
public:
	wWorkerIpc(wWorker *worker);
	virtual ~wWorkerIpc();

    const wStatus& PrepareStart();

    virtual const wStatus& RunThread();

    virtual const wStatus& PrepareRun() {
    	return mStatus;
    }

    virtual const wStatus& Run() {
    	return mStatus;
    }

protected:
    friend class wWorker;

    static uint32_t Shard(wSocket* sock) {
        uint32_t hash = misc::Hash(sock->Host().c_str(), sock->Host().size(), 0);
        return hash >> (32 - kChannelNumShard);
    }

    // 事件读写主调函数
    const wStatus& Recv();

    const wStatus& InitEpoll();

    // 添加channel socket到epoll侦听事件队列
    const wStatus& Channel2Epoll(bool addpool = true);

    const wStatus& AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool addpool = true);
    const wStatus& RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL, bool delpool = true);
    const wStatus& CleanTask();

    const wStatus& AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    const wStatus& CleanTaskPool(std::vector<wTask*> pool);

    int32_t mEpollFD;
    uint64_t mTimeout;

    // task|pool
    wTask *mTask;
    std::vector<wTask*> mTaskPool[kChannelNumShard];
    wMutex mTaskPoolMutex[kChannelNumShard];

    // 当前进程
    wWorker *mWorker;

    wStatus mStatus;
};

}	// namespace hnet

#endif
