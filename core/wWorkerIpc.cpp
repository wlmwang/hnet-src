
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include <sys/epoll.h>
#include "wWorkerIpc.h"
#include "wWorker.h"
#include "wMisc.h"
#include "wChannelSocket.h"
#include "wChannelTask.h"

namespace hnet {

wWorkerIpc::wWorkerIpc(wWorker *worker) : mWorker(worker), mEpollFD(kFDUnknown), mTimeout(10), mTaskCount(0), mTask(NULL) {
	mEpollEventPool.reserve(kListenBacklog);
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
}

wWorkerIpc::~wWorkerIpc() {
	CleanEpoll();
	CleanTaskPool();
}

wStatus wWorkerIpc::PrepareRun() {
	if (!InitEpoll().Ok()) {
		//return mStatus;
		exit(2);
	}

	// worker自身channel[1]被监听
	if (mWorker != NULL && mWorker->mWorkerPool[mWorker->mSlot] != NULL) {
		wChannelSocket *channel = mWorker->mWorkerPool[mWorker->mSlot]->mChannel;
		if (channel != NULL) {
			wTask *task;
			SAFE_NEW(wChannelTask(channel, mWorker), task);
			if (task == NULL) {
				//return mStatus = wStatus::IOError("wWorkerIpc::PrepareRun failed", "new failed");
				exit(2);
			}

			if (!AddToEpoll(task).Ok() || !AddToTaskPool(task).Ok()) {
				SAFE_DELETE(task);
				exit(2);
			}
		} else {
			mStatus = wStatus::IOError("wWorkerIpc::PrepareRun, channel invalid", "");
			exit(2);
		}
	} else {
		mStatus = wStatus::IOError("wWorkerIpc::PrepareRun, worker start thread failed", "worker channel fd invalid");
		exit(2);
	}

	return mStatus;
}

wStatus wWorkerIpc::Run() {
	while (mStatus.Ok()) {
		Recv();
	}
	return mStatus;
}

wStatus wWorkerIpc::InitEpoll() {
	if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
		return mStatus = wStatus::IOError("wWorkerIpc::InitEpoll, epoll_create() failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wWorkerIpc::AddToEpoll(wTask* task, int ev, int op) {
	mEpollEvent.events = ev | EPOLLERR | EPOLLHUP; // |EPOLLET
	mEpollEvent.data.fd = task->mSocket->mFD;
	mEpollEvent.data.ptr = task;
	if (epoll_ctl(mEpollFD, op, task->mSocket->mFD, &mEpollEvent) < 0) {
		return mStatus = wStatus::IOError("wWorkerIpc::AddToEpoll, epoll_ctl() failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wWorkerIpc::AddToTaskPool(wTask* task) {
	mTaskPool.push_back(task);
	mTaskCount = mTaskPool.size();
	if (mTaskCount > mEpollEventPool.capacity()) {
		mEpollEventPool.reserve(mTaskCount * 2);
	}
	return mStatus = wStatus::Nothing();
}

wStatus wWorkerIpc::CleanEpoll() {
	if (close(mEpollFD) == -1) {
		return mStatus = wStatus::IOError("wWorkerIpc::CleanEpoll, close() failed", strerror(errno));
	}
	mEpollFD = -1;
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
	mEpollEventPool.clear();
}

wStatus wWorkerIpc::CleanTaskPool() {
	if (mTaskPool.size() > 0) {
		for (vector<wTask*>::iterator it = mTaskPool.begin(); it != mTaskPool.end(); it++) {
			SAFE_DELETE(*it);
		}
	}
	mTaskPool.clear();
	mTaskCount = 0;
	return mStatus = wStatus::Nothing();
}

wStatus wWorkerIpc::RemoveEpoll(wTask* task) {
	int iFD = task->Socket()->FD();
	mEpollEvent.data.fd = iFD;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, iFD, &mEpollEvent) < 0) {
		return mStatus = wStatus::IOError("wWorkerIpc::RemoveEpoll, epoll_ctl() failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

std::vector<wTask*>::iterator wWorkerIpc::RemoveTaskPool(wTask* task) {
    std::vector<wTask*>::iterator it = std::find(mTaskPool.begin(), mTaskPool.end(), task);
    if (it != mTaskPool.end()) {
    	SAFE_DELETE(*it);
        it = mTaskPool.erase(it);
    }
    mTaskCount = mTaskPool.size();
    return it;
}

wStatus wWorkerIpc::Recv() {
	int iRet = epoll_wait(mEpollFD, &mEpollEventPool[0], mTaskCount, mTimeout);
	if (iRet == -1) {
		return mStatus = wStatus::IOError("wWorkerIpc::Recv, epoll_wait() failed", strerror(errno));
	}
	
	wTask *task = NULL;
	for (int i = 0 ; i < iRet ; i++) {
		task = reinterpret_cast<wTask *>(mEpollEventPool[i].data.ptr);
		int64_t fd = task->Socket()->FD();
		
		if (fd == kFDUnknown) {
			if (!RemoveEpoll(task).Ok() || !RemoveTaskPool(task).Ok()) {
				return mStatus;
			}
			continue;
		}
		if (!task->IsRunning()) {	// 多数是超时设置
			if (!RemoveEpoll(task).Ok() || !RemoveTaskPool(task).Ok()) {
				return mStatus;
			}
			continue;
		}

		// 出错(多数为sock已关闭)
		if (mEpollEventPool[i].events & (EPOLLERR | EPOLLPRI)) {
			if (!RemoveEpoll(task).Ok() || !RemoveTaskPool(task).Ok()) {
				return mStatus;
			}
			continue;
		}
		
		if (mEpollEventPool[i].events & EPOLLIN) {
			// 套接口准备好了读取操作
			mStatus = task->TaskRecv();
			if (!mStatus.Ok()) {
				if (!RemoveEpoll(task).Ok() || !RemoveTaskPool(task).Ok()) {
					return mStatus;
				}
			}
		} else if (mEpollEventPool[i].events & EPOLLOUT) {
			// 清除写事件
			if (task->SendLen() == 0) {
				AddToEpoll(task, EPOLLIN, EPOLL_CTL_MOD);
				continue;
			}
			// 套接口准备好了写入操作
			// 写入失败，半连接，对端读关闭
			mStatus = task->TaskSend();
			if (!mStatus.Ok()) {
				if (!RemoveEpoll(task).Ok() || !RemoveTaskPool(task).Ok()) {
					return mStatus;
				}
			}
		}
	}
}

}	// namespace hnet
