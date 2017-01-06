
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorkerIpc.h"
#include "wChannelSocket.h"
#include "wChannelTask.h"

namespace hnet {

wWorkerIpc::wWorkerIpc(wWorker *worker) : mTick(0), mHeartbeatTurn(kHeartbeatTurn), mScheduleOk(true),
mEpollFD(kFDUnknown), mTimeout(10), mTask(NULL), mWorker(worker) {
	assert(mWorker != NULL);
    mLatestTm = misc::GetTimeofday();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wWorkerIpc::~wWorkerIpc() {
    CleanTask();
}

const wStatus& wWorkerIpc::PrepareStart() {
    if (!InitEpoll().Ok()) {
       return mStatus;
    } else if (!Channel2Epoll(true).Ok()) {
		return mStatus;
    }
    return PrepareRun();
}

const wStatus& wWorkerIpc::Start() {
    // 进入服务主服务
    while (true) {
        Recv();
        Run();
        CheckTick();
    }
    return mStatus;
}

const wStatus& wWorkerIpc::RunThread() {
	return Start();
}

const wStatus& wWorkerIpc::InitEpoll() {
    if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
       return mStatus = wStatus::IOError("wWorkerIpc::InitEpoll, epoll_create() failed", error::Strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wWorkerIpc::Channel2Epoll(bool addpool) {
	// channel socket加入epoll写入事件
	wChannelSocket *socket = mWorker->Channel();
	if (socket != NULL) {
		wTask *task;
		mWorker->NewChannelTask(socket, &task);
		if (mStatus.Ok()) {
			AddTask(task, EPOLLIN, EPOLL_CTL_ADD, addpool);
		}
	} else {
		mStatus = wStatus::Corruption("wWorkerIpc::Channel2Epoll failed", "channel is null");
	}
	return mStatus;
}

const wStatus& wWorkerIpc::AddTask(wTask* task, int ev, int op, bool addpool) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP | EPOLLET;
    evt.data.fd = task->Socket()->FD();
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt) == -1) {
        return mStatus = wStatus::IOError("wWorkerIpc::AddTask, epoll_ctl() failed", error::Strerror(errno));
    }
    if (addpool) {
        AddToTaskPool(task);
    }
    return mStatus;
}

const wStatus& wWorkerIpc::RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter, bool delpool) {
    struct epoll_event evt;
    evt.data.fd = task->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt) < 0) {
		return mStatus = wStatus::IOError("wWorkerIpc::RemoveTask, epoll_ctl() failed", error::Strerror(errno));
    }
    if (delpool) {
    	// 清除客户端对象
        std::vector<wTask*>::iterator it = RemoveTaskPool(task);
        if (iter != NULL) {
        	*iter = it;
        }
    }
    return mStatus;
}

const wStatus& wWorkerIpc::CleanTask() {
    if (close(mEpollFD) == -1) {
        mStatus = wStatus::IOError("wWorkerIpc::CleanTask, close() failed", error::Strerror(errno));
    }
    mEpollFD = kFDUnknown;
    for (int i = 0; i < kChannelNumShard; i++) {
        CleanTaskPool(mTaskPool[i]);
    }
    return mStatus;
}

const wStatus& wWorkerIpc::AddToTaskPool(wTask* task) {
    mTaskPool[task->Type()].push_back(task);
    return mStatus;
}

std::vector<wTask*>::iterator wWorkerIpc::RemoveTaskPool(wTask* task) {
	int32_t type =  task->Type();
    std::vector<wTask*>::iterator it = std::find(mTaskPool[type].begin(), mTaskPool[type].end(), task);
    if (it != mTaskPool[type].end()) {
    	SAFE_DELETE(*it);
        it = mTaskPool[type].erase(it);
    }
    return it;
}

const wStatus& wWorkerIpc::CleanTaskPool(std::vector<wTask*> pool) {
    if (pool.size() > 0) {
        for (std::vector<wTask*>::iterator it = pool.begin(); it != pool.end(); it++) {
            SAFE_DELETE(*it);
        }
    }
    pool.clear();
    return mStatus;
}

void wServer::CheckTick() {
	if (mScheduleMutex.TryLock() == 0) {
		mTick = misc::GetTimeofday() - mLatestTm;
		if (mTick >= 10*1000) {
		    mLatestTm += mTick;
		    // 添加任务到线程池中
		    if (mScheduleOk == true) {
		    	mScheduleOk = false;
		    	wEnv::Default()->Schedule(wServer::ScheduleRun, this);
		    }
		}
	}
	mScheduleMutex.Unlock();
}

const wStatus& wWorkerIpc::Recv() {
    std::vector<struct epoll_event> evt(kListenBacklog);
    int ret = epoll_wait(mEpollFD, &evt[0], kListenBacklog, mTimeout);
    if (ret == -1) {
       return mStatus = wStatus::IOError("wWorkerIpc::Recv, epoll_wait() failed", error::Strerror(errno));
    }

    wTask* task;
    ssize_t size;
    for (int i = 0 ; i < ret ; i++) {
        task = reinterpret_cast<wTask*>(evt[i].data.ptr);
        // 加锁
        int type = task->Type();
        mTaskPoolMutex[type].Lock();
        if (evt[i].events & (EPOLLERR | EPOLLPRI)) {
        	task->Socket()->SS() = kSsUnconnect;
        } else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
            if (evt[i].events & EPOLLIN) {
                // 套接口准备好了读取操作
                if (!(mStatus = task->TaskRecv(&size)).Ok()) {
                	task->Socket()->SS() = kSsUnconnect;
                }
            } else if (evt[i].events & EPOLLOUT) {
                // 清除写事件
                if (task->SendLen() == 0) {
                    AddTask(task, EPOLLIN, EPOLL_CTL_MOD, false);
                } else {
                    // 套接口准备好了写入操作
                    // 写入失败，半连接，对端读关闭
                    if (!(mStatus = task->TaskSend(&size)).Ok()) {
                    	task->Socket()->SS() = kSsUnconnect;
                    }
                }
            }
        }
		// 解锁
		mTaskPoolMutex[type].Unlock();
    }
    return mStatus;
}

}
