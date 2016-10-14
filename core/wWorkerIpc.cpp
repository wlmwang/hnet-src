
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include "wWorkerIpc.h"
#include "wWorker.h"
#include "wMaster.h"
#include "wMisc.h"
#include "wChannelSocket.h"
#include "wChannelTask.h"

namespace hnet {

wWorkerIpc::wWorkerIpc(wWorker *worker) : mWorker(worker), mEpollFD(kFDUnknown), mTimeout(10), mTask(NULL) { }

wWorkerIpc::~wWorkerIpc() {
	CleanTask();
}

wStatus wWorkerIpc::PrepareRun() {
	if (!InitEpoll().Ok()) {
		return mStatus;
	}

	// worker自身channel[1]被监听
	if (mWorker != NULL && mWorker->Master() != NULL && mWorker->Master()->Worker(mWorker->mSlot) != NULL) {
		wChannelSocket *socket = mWorker->Master()->Worker(mWorker->mSlot)->Channel();
		if (socket != NULL) {
			wTask *task;
			SAFE_NEW(wChannelTask(socket, mWorker), task);
			if (task == NULL) {
				return mStatus = wStatus::IOError("wWorkerIpc::PrepareRun failed", "new failed");
			} else if (!AddTask(task, EPOLLIN, EPOLL_CTL_ADD, true).Ok()) {
				SAFE_DELETE(task);
			}
		} else {
			mStatus = wStatus::IOError("wWorkerIpc::PrepareRun failed", "channel is null");
		}
	} else {
		mStatus = wStatus::IOError("wWorkerIpc::PrepareRun, worker start thread failed", "worker channel fd invalid");
	}

	return mStatus;
}

wStatus wWorkerIpc::Run() {
	while (true) {
		Recv();
	}
	return mStatus;
}

wStatus wWorkerIpc::InitEpoll() {
	if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
		return mStatus = wStatus::IOError("wWorkerIpc::InitEpoll, epoll_create() failed", strerror(errno));
	}
	return mStatus;
}

wStatus wWorkerIpc::AddTask(wTask* task, int ev, int op, bool newconn) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP | EPOLLET;
    evt.data.fd = task->Socket()->FD();
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt) == -1) {
		return mStatus = wStatus::IOError("wWorkerIpc::AddTask, epoll_ctl() failed", strerror(errno));
    }
    if (newconn) {
    	AddToTaskPool(task);
    }
    return mStatus;
}

wStatus wWorkerIpc::RemoveTask(wTask* task) {
    struct epoll_event evt;
    evt.data.fd = task->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt) < 0) {
		return mStatus = wStatus::IOError("wWorkerIpc::RemoveTask, epoll_ctl() failed", strerror(errno));
    }
    RemoveTaskPool(task);
    return mStatus;
}

wStatus wWorkerIpc::CleanTask() {
    if (close(mEpollFD) == -1) {
		return mStatus = wStatus::IOError("wServer::CleanTask, close() failed", strerror(errno));
    }
    mEpollFD = kFDUnknown;
    CleanTaskPool();
    return mStatus;
}

wStatus wWorkerIpc::AddToTaskPool(wTask* task) {
    mTaskPool.push_back(task);
    return mStatus;
}

std::vector<wTask*>::iterator wWorkerIpc::RemoveTaskPool(wTask* task) {
    std::vector<wTask*>::iterator it = std::find(mTaskPool.begin(), mTaskPool.end(), task);
    if (it != mTaskPool.end()) {
    	SAFE_DELETE(*it);
        it = mTaskPool.erase(it);
    }
    return it;
}

wStatus wWorkerIpc::CleanTaskPool() {
    if (mTaskPool.size() > 0) {
		for (std::vector<wTask*>::iterator it = mTaskPool.begin(); it != mTaskPool.end(); it++) {
		    SAFE_DELETE(*it);
		}
    }
    mTaskPool.clear();
    return mStatus;
}

wStatus wWorkerIpc::Recv() {
    std::vector<struct epoll_event> evt(kListenBacklog);
    int iRet = epoll_wait(mEpollFD, &evt[0], kListenBacklog, mTimeout);
    if (iRet == -1) {
		return mStatus = wStatus::IOError("wWorkerIpc::Recv, epoll_wait() failed", strerror(errno));
    }

    wTask *task;
    ssize_t size;
	for (int i = 0 ; i < iRet ; i++) {
		task = reinterpret_cast<wTask *>(evt[i].data.ptr);
		int64_t fd = task->Socket()->FD();
		
		if (fd == kFDUnknown) {
		    if (!RemoveTask(task).Ok()) {
				return mStatus;
		    }
		} else if (evt[i].events & (EPOLLERR | EPOLLPRI)) {
		    if (!RemoveTask(task).Ok()) {
				return mStatus;
		    }
		} else if (evt[i].events & EPOLLIN) {
			// 套接口准备好了读取操作
			mStatus = task->TaskRecv(&size);
			if (!mStatus.Ok()) {
			    if (!RemoveTask(task).Ok()) {
					return mStatus;
			    }
			}
	    } else if (evt[i].events & EPOLLOUT) {
			// 清除写事件
			if (task->SendLen() == 0) {
			    AddTask(task, EPOLLIN, EPOLL_CTL_MOD, false);
			    continue;
			}
			// 套接口准备好了写入操作
			// 写入失败，半连接，对端读关闭
			mStatus = task->TaskSend(&size);
			if (!mStatus.Ok()) {
			    if (!RemoveTask(task).Ok()) {
			        return mStatus;
			    }
			}
	    }
	}
	return mStatus;
}

}	// namespace hnet
