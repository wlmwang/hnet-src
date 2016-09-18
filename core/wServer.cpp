
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wServer.h"
#include "wMisc.h"
#include "wLog.h"
#include "wTimer.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"

namespace hnet {

wServer::wServer(string title) : mTitle(title), mExiting(false), mTimerSwitch(false), mEpollFD(kFDUnknown), 
mEnv(wEnv::Default()), mTimeout(10), mTaskCount(0), mTask(NULL) {
	mLatestTm = misc::GetTimeofday();
	mHeartbeatTimer = wTimer(kKeepAliveTm);
	mEpollEventPool.reserve(kListenBacklog);
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
}

wServer::~wServer(){
	CleanEpoll();
	CleanTaskPool();
}

wStatus wServer::PrepareSingle(string ipaddr, uint16_t port, string protocol) {	
	if (!AddListener(ipaddr, port, protocol).Ok()) {
		return mStatus;
	} else if (!InitEpoll().Ok()) {
		return mStatus;
	} else if (!Listener2Epoll().Ok()) {
		return mStatus;
	}

	return PrepareRun();
}

wStatus wServer::SingleStart(bool bDaemon) {
	// 进入服务主循环
	while (bDaemon) {
		Recv();
		Run();
		CheckTimer();
	}
	return mStatus;
}

wStatus wServer::PrepareMaster(string ipaddr, uint16_t port, string protocol) {	
	if (!AddListener(ipaddr, port, protocol).Ok()) {
		return mStatus;
	}
	return PrepareRun();
}

wStatus wServer::WorkerStart(bool bDaemon) {
	if (!InitEpoll().Ok()) {
		return mStatus;
	} else if (!Listener2Epoll().Ok()) {
		return mStatus;
	}

	// 开启心跳线程
	mEnv->Schedule(wServer::CheckTimer, this);

	// 进入服务主循环
	while (bDaemon) {
		if (mExiting) {
			WorkerExit();
		}

		Recv();
		HandleSignal();
		Run();
		//CheckTimer();
	}
	return mStatus;
}

void wServer::HandleSignal() {	
	if (g_terminate) {
		// 直接退出
		WorkerExit();
	}
	
	if (g_quit)	{
		// 平滑退出
		g_quit = 0;

		//LOG_ERROR(ELOG_KEY, "[system] gracefully shutting down");
		if (!mExiting) {
			mExiting = true;
			for (std::vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) {
				(*it)->Close();
			}
		}
	}
}

void wServer::WorkerExit() {
	if (mExiting) {
		// 关闭连接池
		// todo
	}

	LOG_ERROR(ELOG_KEY, "[system] worker process exiting");
	exit(0);
}

wStatus wServer::InitEpoll() {
	if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
		return mStatus = wStatus::IOError("wServer::InitEpoll, epoll_create() failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wServer::NewTcpTask(wSocket* sock, wTask** ptr) {
    SAFE_NEW(wTcpTask(sock), *ptr);
    if (*ptr == NULL) {
		return wStatus::IOError("wServer::NewTcpTask", "new failed");
    }
    return wStatus::Nothing();
}

wStatus wServer::NewUnixTask(wSocket* sock, wTask** ptr) {
    SAFE_NEW(wUnixTask(sock), *ptr);
    if (*ptr == NULL) {
		return wStatus::IOError("wServer::NewUnixTask", "new failed");
    }
    return wStatus::Nothing();
}

wStatus wServer::AddToEpoll(wTask* task, int ev, int op) {
	mEpollEvent.events = ev | EPOLLERR | EPOLLHUP; // |EPOLLET
	mEpollEvent.data.fd = task->mSocket->mFD;
	mEpollEvent.data.ptr = task;
	if (epoll_ctl(mEpollFD, op, task->mSocket->mFD, &mEpollEvent) < 0) {
		return mStatus = wStatus::IOError("wServer::AddToEpoll, epoll_ctl() failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wServer::AddToTaskPool(wTask* task) {
	mTaskPool.push_back(task);
	mTaskCount = mTaskPool.size();
	if (mTaskCount > mEpollEventPool.capacity()) {
		mEpollEventPool.reserve(mTaskCount * 2);
	}
	return mStatus = wStatus::Nothing();
}

wStatus wServer::Recv() {
	int iRet = epoll_wait(mEpollFD, &mEpollEventPool[0], mTaskCount, mTimeout);
	if (iRet == -1) {
		return mStatus = wStatus::IOError("wServer::Recv, epoll_wait() failed", strerror(errno));
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
		
		if (task->Socket()->SockType() == kStListen && task->Socket()->SockStatus() == kSsListened) {
			if (mEpollEventPool[i].events & EPOLLIN) {
				// accept connect
				mStatus = AcceptConn(task);
			} else {
				mStatus = wStatus::IOError("wServer::Recv, accept error", "listen socket error event");
			}
		} else if (task->Socket()->SockType() == kStConnect && task->Socket()->SockStatus() == kSsConnected) {
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
}

int wServer::Send(wTask *pTask, const char *pCmd, int iLen) {
	W_ASSERT(pTask != NULL && pTask->IsRunning(), return -1);

	if (pTask->Socket()->SockType() == SOCK_TYPE_CONNECT && pTask->Socket()->SockStatus() == SOCK_STATUS_CONNECTED && (pTask->Socket()->SockFlag() == SOCK_FLAG_SEND || pTask->Socket()->SockFlag() == SOCK_FLAG_RVSD)) {
		if (pTask->Send2Buf(pCmd, iLen) > 0) {
			return AddToEpoll(pTask, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD);
		}
	}
	return -1;
}

void wServer::Broadcast(const char *pCmd, int iLen) {
	W_ASSERT(pCmd != NULL, return);

	if (mTaskPool.size() > 0) {
		for (vector<wTask*>::iterator iter = mTaskPool.begin(); iter != mTaskPool.end(); iter++) {
			if (*iter != NULL) Send(*iter, pCmd, iLen);
		}
	}
}

wStatus wServer::AddListener(string ipaddr, uint16_t port, string protocol) {
	wSocket *socket = NULL;
	if (protocol == "TCP") {
		SAFE_NEW(wTcpSocket(kStListen), socket);
	} else if(protocol == "UNIX") {
		SAFE_NEW(wUnixSocket(kStListen), socket);
	}

	if (socket != NULL) {
		mStatus = socket->Open();
		if (!mStatus.Ok()) {
			SAFE_DELETE(socket);
			return mStatus;
		}
		mStatus = socket->Listen(ipaddr, port);
		if (!mStatus.Ok()) {
			SAFE_DELETE(socket);
			return mStatus;
		}
		socket->mSockStatus = kSsListened;
		mListenSock.push_back(socket);
	} else {
		return mStatus = wStatus::IOError("wServer::AddListener", "new failed");
	}
	return mStatus = wStatus::Nothing();
}

wStatus wServer::Listener2Epoll() {
	// 添加到侦听事件队列
	for (std::vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) {
		switch ((*it)->mSockProto) {
		case kSpUnix:
			mStatus = NewUnixTask(*it, mTask);
			break;
		case kSpTcp:
			mStatus = NewTcpTask(*it, mTask);
			break;
		default:
			mStatus = wStatus::IOError("wServer::Listener2Epoll", "unknown task");
		}

		if (mStatus.Ok()) {
			if (!AddToEpoll(mTask).Ok() || !AddToTaskPool(mTask).Ok()) {
				SAFE_DELETE(mTask);
			}
		} else {
			break;
		}
	}
	return mStatus;
}

wStatus wServer::AcceptConn(wTask *task) {
	int64_t fd;
	if (task->Socket()->SockProto() == kSpUnix) {
		struct sockaddr_un sockAddr;
		socklen_t sockAddrSize = sizeof(sockAddr);
		mStatus = task->Socket()->Accept(fd, (struct sockaddr*)&sockAddr, &sockAddrSize);
		if (!mStatus.Ok()) {
			return mStatus;
		}
	    
	    //unix socket
		wUnixSocket *socket;
		SAFE_NEW(wUnixSocket(kStConnect), socket);
		socket->FD() = fd;
		socket->Host() = sockAddr.sun_path;
		socket->SockStatus() = kSsConnected;
		mStatus = socket->SetFL();
		if (!mStatus.Ok()) {
			return mStatus;
		}
		mStatus = NewUnixTask(socket, mTask);

	} else if(task->Socket()->SockProto() == kSpTcp) {
		struct sockaddr_in sockAddr;
		socklen_t sockAddrSize = sizeof(sockAddr);	
		mStatus = task->Socket()->Accept(fd, (struct sockaddr*)&sockAddr, &sockAddrSize);
		if (!mStatus.Ok()) {
			return mStatus;
		}

		//tcp socket
		wTcpSocket *socket;
		SAFE_NEW(wTcpSocket(kStConnect), socket);
		socket->FD() = fd;
		socket->Host() = inet_ntoa(sockAddr.sin_addr);
		socket->Port() = sockAddr.sin_port;
		socket->SockStatus() = kSsConnected;
		mStatus = socket->SetFL();
		if (!mStatus.Ok()) {
			return mStatus;
		}
		mStatus = NewTcpTask(socket, mTask);
	} else {
		mStatus = wStatus::IOError("wServer::Listener2Epoll", "unknown task")
	}
	
	if (mStatus.Ok()) {
		if (!mTask->Login().Ok()) {
			SAFE_DELETE(mTask);
		} else if (!AddToEpoll(mTask).Ok() || !AddToTaskPool(mTask).Ok()) {
			SAFE_DELETE(mTask);
		} else {
			mStatus = wStatus::Nothing();
		}
	}
	return mStatus;
}

wStatus wServer::CleanEpoll() {
	if (close(mEpollFD) == -1) {
		return mStatus = wStatus::IOError("wServer::CleanEpoll, close() failed", strerror(errno));
	}
	mEpollFD = -1;
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
	mEpollEventPool.clear();
}

wStatus wServer::CleanTaskPool() {
	if (mTaskPool.size() > 0) {
		for (std::vector<wTask*>::iterator it = mTaskPool.begin(); it != mTaskPool.end(); it++) {
			SAFE_DELETE(*it);
		}
	}
	mTaskPool.clear();
	mTaskCount = 0;
	return mStatus = wStatus::Nothing();
}

wStatus wServer::RemoveEpoll(wTask* task) {
	int iFD = task->Socket()->FD();
	mEpollEvent.data.fd = iFD;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, iFD, &mEpollEvent) < 0) {
		return mStatus = wStatus::IOError("wServer::RemoveEpoll, epoll_ctl() failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

std::vector<wTask*>::iterator wServer::RemoveTaskPool(wTask* task) {
    std::vector<wTask*>::iterator it = std::find(mTaskPool.begin(), mTaskPool.end(), task);
    if (it != mTaskPool.end()) {
    	SAFE_DELETE(*it);
        it = mTaskPool.erase(it);
    }
    mTaskCount = mTaskPool.size();
    return it;
}

void wServer::CheckTimer(void* arg) {
	uint64_t interval = misc::GetTimeofday() - mLatestTm;
	if (interval < 100) {
		return;
	}
	mLatestTm += interval;

	wServer* server = reinterpret_cast<wServer* >(arg);
	if (server->mHeartbeatTimer.CheckTimer(interval)) {
		server->CheckTimeout();
	}
}

void wServer::CheckTimeout() {
	if (!mTimerSwitch) {
		return;
	}

	uint64_t nowTm = misc::GetTimeofday();
	if (mTaskPool.size() > 0) {
		for (std::vector<wTask*>::iterator iter = mTaskPool.begin(); iter != mTaskPool.end(); iter++) {
			if ((*iter)->Socket()->SockType() == kStConnect && (*iter)->Socket()->SockStatus() == kSsConnected) {
				// 上一次发送时间间隔
				uint64_t interval = nowTm - (*iter)->Socket()->SendTime();
				if (interval >= kKeepAliveTm) {
					bool bDelTask = false;
					(*iter)->Heartbeat();
					if ((*iter)->HeartbeatOutTimes()) {
						bDelTask = true;
					}
					// 关闭超时连接
					if (bDelTask) {
						if (RemoveEpoll(task).Ok() && RemoveTaskPool(task).Ok()) {
							iter--;
						}
					}
				}
			}
		}
	}
}

}	// namespace hnet