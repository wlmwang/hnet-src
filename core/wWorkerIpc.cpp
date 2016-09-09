
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

wWorkerIpc::wWorkerIpc(wWorker *pWorker) : mWorker(pWorker), mEpollFD(kFDUnknown), mTimeout(10), mTaskCount(0), mTask(NULL) {
	mEpollEventPool.reserve(kListenBacklog);
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
}

wWorkerIpc::~wWorkerIpc() {
	CleanEpoll();
	CleanTaskPool();
}

wStatus wWorkerIpc::NewChannelTask(wChannelSocket* sock, wWorker* worker, wTask** ptr) {
	try {
		*ptr = new wChannelTask(sock, worker);
	} catch (...) {
		return mStatus = wStatus::IOError("wWorkerIpc::NewChannelTask", "new failed");
	}
	return mStatus = wStatus::Nothing();
}

wStatus wWorkerIpc::PrepareRun() {
	//LOG_INFO(ELOG_KEY, "[system] worker process sync-channel's thread prepare start");

	if (!InitEpoll().Ok()) {
		return mStatus;
		exit(2);
	}

	// worker自身channel[1]被监听
	if (mWorker != NULL && mWorker->mWorkerPool[mWorker->mSlot] != NULL) {
		wChannel *channel = mWorker->mWorkerPool[mWorker->mSlot]->mChannel;
		if (channel != NULL) {
			wTask *pTask;
			mStatus = NewChannelTask(channel, mWorker, &pTask);
			if (!mStatus.Ok()) {
				return mStatus;
			}

			if (AddToEpoll(pTask) >= 0) {
				AddToTaskPool(pTask);
			} else {
				misc::SafeDelete(pTask);
				exit(2);
			}
		} else {
			mStatus = wStatus::IOError("wWorkerIpc::PrepareRun, channel invalid", "");
			exit(2);
		}
	} else {
		mStatus = wStatus::IOError("wWorkerIpc::PrepareRun, worker start thread failed", "worker channel fd invalid");
	}

	return mStatus;
}

wStatus wWorkerIpc::Run() {
	//LOG_INFO(ELOG_KEY, "[system] worker process sync-channel's thread start");
	//mStatus = SERVER_RUNNING;

	do {
		Recv();
	} while (IsRunning());

	return mStatus = wStatus::Nothing();
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
			misc::SafeDelete(*it);
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
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] sync-channel's epoll remove socket fd(%d) error : %s", iFD, strerror(mErr));
		return -1;
	}
	return 0;
}

std::vector<wTask*>::iterator wWorkerIpc::RemoveTaskPool(wTask* task)
{
    std::vector<wTask*>::iterator it = std::find(mTaskPool.begin(), mTaskPool.end(), task);
    if (it != mTaskPool.end()) {
    	(*it)->CloseTask();
        it = mTaskPool.erase(it);
    }
    mTaskCount = mTaskPool.size();
    return it;
}

void wWorkerIpc::Recv()
{
	int iRet = epoll_wait(mEpollFD, &mEpollEventPool[0], mTaskCount, mTimeout);
	if (iRet < 0) {
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] sync-channel's epoll_wait failed: %s", strerror(mErr));
		return;
	}
	
	int iFD = FD_UNKNOWN;
	int iLenOrErr;
	wTask *pTask = NULL;
	for (int i = 0 ; i < iRet ; i++) {
		pTask = (wTask *)mEpollEventPool[i].data.ptr;
		iFD = pTask->Socket()->FD();
		
		if (iFD == FD_UNKNOWN) {
			LOG_DEBUG(ELOG_KEY, "[system] sync-channel's socket FD is error, fd(%d), close it", iFD);
			if (RemoveEpoll(pTask) >= 0) {
				RemoveTaskPool(pTask);
			}
			continue;
		}
		if (!pTask->IsRunning()) {	// 多数是超时设置
			LOG_DEBUG(ELOG_KEY, "[system] sync-channel's task status is quit, fd(%d), close it", iFD);
			if (RemoveEpoll(pTask) >= 0) {
				RemoveTaskPool(pTask);
			}
			continue;
		}
		if (mEpollEventPool[i].events & (EPOLLERR | EPOLLPRI)) { //出错(多数为sock已关闭)
			mErr = errno;
			LOG_ERROR(ELOG_KEY, "[system] sync-channel's epoll event recv error from fd(%d), close it: %s", iFD, strerror(mErr));
			if (RemoveEpoll(pTask) >= 0) {
				RemoveTaskPool(pTask);
			}
			continue;
		}
		
		if (mEpollEventPool[i].events & EPOLLIN) {
			//套接口准备好了读取操作
			if ((iLenOrErr = pTask->TaskRecv()) < 0) {
				if (iLenOrErr == ERR_CLOSED) {
					LOG_DEBUG(ELOG_KEY, "[system] sync-channel's socket closed by client");
				} else if (iLenOrErr == ERR_MSGLEN) {
					LOG_ERROR(ELOG_KEY, "[system] sync-channel's recv message invalid len");
				} else {
					LOG_ERROR(ELOG_KEY, "[system] EPOLLIN(read) failed or sync-channel's socket closed: %s", strerror(pTask->Socket()->Errno()));
				}
				if (RemoveEpoll(pTask) >= 0) {
					RemoveTaskPool(pTask);
				}
			}
		} else if (mEpollEventPool[i].events & EPOLLOUT) {
			//清除写事件
			if (pTask->WritableLen() <= 0) {
				AddToEpoll(pTask, EPOLLIN, EPOLL_CTL_MOD);
				continue;
			}
			//套接口准备好了写入操作
			if (pTask->TaskSend() < 0) {	//写入失败，半连接，对端读关闭
				LOG_ERROR(ELOG_KEY, "[system] EPOLLOUT(write) failed or sync-channel's socket closed: %s", strerror(pTask->Socket()->Errno()));
				if (RemoveEpoll(pTask) >= 0)
				{
					RemoveTaskPool(pTask);
				}
			}
		}
	}
}

}	// namespace hnet
