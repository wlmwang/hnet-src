
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wServer.h"
#include "wMisc.h"
#include "wMutex.h"
#include "wTimer.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"

namespace hnet {

wServer::wServer(string title) : mTitle(title), mExiting(false), mCheckSwitch(false), mEpollFD(kFDUnknown), 
mEnv(wEnv::Default()), mTimeout(10), mTask(NULL) {
    mLatestTm = misc::GetTimeofday();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
    SAFE_NEW(wMutex, mTaskPoolMutex);
}

wServer::~wServer(){
    CleanTask();
    SAFE_DELETE(mTaskPoolMutex);
}

wStatus wServer::Prepare(string ipaddr, uint16_t port, string protocol) {	
    if (!AddListener(ipaddr, port, protocol).Ok()) {
	return mStatus;
    }
    return mStatus = PrepareRun();
}

wStatus wServer::SingleStart(bool daemon) {
    if (!InitEpoll().Ok()) {
	return mStatus;
    } else if (!Listener2Epoll().Ok()) {
	return mStatus;
    }
    
    // 开启心跳线程
    mEnv->Schedule(wServer::CheckTimer, this);
    
    // 进入服务主循环
    while (daemon) {
	Recv();
	Run();
    }
    return mStatus;
}

wStatus wServer::WorkerStart(bool daemon) {
    if (!InitEpoll().Ok()) {
	return mStatus;
    } else if (!Listener2Epoll().Ok()) {
	return mStatus;
    }
    
    // 开启心跳线程
    mEnv->Schedule(wServer::CheckTimer, this);
    
    // 进入服务主循环
    while (daemon) {
	if (mExiting) {
	    WorkerExit();
	}
	
	Recv();
	HandleSignal();
	Run();
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

	if (!mExiting) {
	    mExiting = true;
	    for (std::vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) {
		SAFE_DELETE(*it);
	    }
	    mListenSock.clear();
	}
    }
}

void wServer::WorkerExit() {
    if (mExiting) {
	CleanTask();
    }
    exit(0);
}

wStatus wServer::NewTcpTask(wSocket* sock, wTask** ptr) {
    SAFE_NEW(wTcpTask(sock), *ptr);
    if (*ptr == NULL) {
	return wStatus::IOError("wServer::NewTcpTask", "new failed");
    }
    return mStatus;
}

wStatus wServer::NewUnixTask(wSocket* sock, wTask** ptr) {
    SAFE_NEW(wUnixTask(sock), *ptr);
    if (*ptr == NULL) {
	return wStatus::IOError("wServer::NewUnixTask", "new failed");
    }
    return mStatus;
}

wStatus wServer::Recv() {
    std::vector<struct epoll_event> evt(kListenBacklog);
    int iRet = epoll_wait(mEpollFD, &evt[0], kListenBacklog, mTimeout);
    if (iRet == -1) {
	return mStatus = wStatus::IOError("wServer::Recv, epoll_wait() failed", strerror(errno));
    }
    
    for (wTask *task = NULL, int i = 0 ; i < iRet ; i++) {
	task = reinterpret_cast<wTask *>(evt[i].data.ptr);
	int64_t fd = task->Socket()->FD();
	if (fd == kFDUnknown) {
	    if (RemoveTask(task).Ok()) {
		return mStatus;
	    }
	} else if (evt[i].events & (EPOLLERR | EPOLLPRI)) {
	    if (RemoveTask(task).Ok()) {
		return mStatus;
	    }
	} else if (task->Socket()->ST() == kStListen && task->Socket()->SS() == kSsListened) {
	    if (evt[i].events & EPOLLIN) {
		mStatus = AcceptConn(task);
	    } else {
		mStatus = wStatus::IOError("wServer::Recv, accept error", "listen socket error event");
	    }
	} else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
	    if (evt[i].events & EPOLLIN) {
		// 套接口准备好了读取操作
		mStatus = task->TaskRecv();
		if (!mStatus.Ok()) {
		    if (RemoveTask(task).Ok()) {
			return mStatus;
		    }
		}
	    } else if (evt[i].events & EPOLLOUT) {
		// 清除写事件
		if (task->SendLen() == 0) {
		    AddToEpoll(task, EPOLLIN, EPOLL_CTL_MOD);
		    continue;
		}
		// 套接口准备好了写入操作
		// 写入失败，半连接，对端读关闭
		mStatus = task->TaskSend();
		if (!mStatus.Ok()) {
		    if (RemoveTask(task).Ok()) {
		        return mStatus;
		    }
		}
	    }
	}
    }
    return mStatus;
}

wStatus wServer::Send(wTask *task, const char *cmd, int len) {
    if (task != NULL && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected && (pTask->Socket()->SF() == kSfSend || pTask->Socket()->SF() == kSfRvsd)) {
	mStatus = task->Send2Buf(cmd, len);
	if (mStatus.Ok()) {
	    return AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
	}
    } else {
	mStatus = wStatus::IOError("wServer::Send, send error", "socket cannot send message");
    }
    return mStatus;
}

wStatus wServer::Broadcast(const char *cmd, int len) {
    if (mTaskPool.size() > 0) {
	for (std::vector<wTask*>::iterator iter = mTaskPool.begin(); iter != mTaskPool.end(); iter++) {
	    Send(*iter, cmd, len);
	}
    }
    return mStatus;
}

wStatus wServer::AcceptConn(wTask *task) {
    if (task->Socket()->SP() == kSpUnix) {
	int64_t fd;
	struct sockaddr_un sockAddr;
	socklen_t sockAddrSize = sizeof(sockAddr);
	mStatus = task->Socket()->Accept(fd, (struct sockaddr*)&sockAddr, &sockAddrSize);
	if (!mStatus.Ok()) {
	    return mStatus;
	}
	
	// unix socket
	wUnixSocket *socket;
	SAFE_NEW(wUnixSocket(kStConnect), socket);
	socket->FD() = fd;
	socket->Host() = sockAddr.sun_path;
	socket->SS() = kSsConnected;
	mStatus = socket->SetFL();
	if (!mStatus.Ok()) {
	    return mStatus;
	}
	mStatus = NewUnixTask(socket, mTask);
    
    } else if(task->Socket()->SockProto() == kSpTcp) {
	int64_t fd;
	struct sockaddr_in sockAddr;
	socklen_t sockAddrSize = sizeof(sockAddr);	
	mStatus = task->Socket()->Accept(fd, (struct sockaddr*)&sockAddr, &sockAddrSize);
	if (!mStatus.Ok()) {
	    return mStatus;
	}

	// tcp socket
	wTcpSocket *socket;
	SAFE_NEW(wTcpSocket(kStConnect), socket);
	socket->FD() = fd;
	socket->Host() = inet_ntoa(sockAddr.sin_addr);
	socket->Port() = sockAddr.sin_port;
	socket->SS() = kSsConnected;
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
	} else if (!AddTask(mTask).Ok()) {
	    SAFE_DELETE(mTask);
	}
    }
    return mStatus;
}

wStatus wServer::InitEpoll() {
    if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
	return mStatus = wStatus::IOError("wServer::InitEpoll, epoll_create() failed", strerror(errno));
    }
    return mStatus;
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
	socket->SS() = kSsListened;
	mListenSock.push_back(socket);
    } else {
	return mStatus = wStatus::IOError("wServer::AddListener", "new failed");
    }
    
    return mStatus;
}

wStatus wServer::Listener2Epoll() {
    for (wTask *task = NULL, std::vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) {
	switch ((*it)->SP()) {
	case kSpUnix:
	    mStatus = NewUnixTask(*it, task);
	    break;
	case kSpTcp:
	    mStatus = NewTcpTask(*it, task);
	    break;
	default:
	    mStatus = wStatus::IOError("wServer::Listener2Epoll", "unknown task");
	}
	if (task != NULL && mStatus.Ok()) {
	    if (!AddTask(task).Ok()) {
		SAFE_DELETE(task);
		break;
	    }
	}
    }
    return mStatus;
}

wStatus wServer::AddTask(wTask* task, int ev, int op, bool newconn) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP; // |EPOLLET
    evt.data.fd = task->Socket()->FD();
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, evt.data.fd, &evt) == -1) {
	return mStatus = wStatus::IOError("wServer::AddToEpoll, epoll_ctl() failed", strerror(errno));
    }
    newconn && AddToTaskPool();
    return mStatus;
}

wStatus wServer::RemoveTask(wTask* task) {
    struct epoll_event evt;
    evt.data.fd = task->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, evt.data.fd, &evt) < 0) {
	return mStatus = wStatus::IOError("wServer::RemoveEpoll, epoll_ctl() failed", strerror(errno));
    }
    RemoveTaskPool(task);
    return mStatus;
}

wStatus wServer::CleanTask() {
    if (close(mEpollFD) == -1) {
	return mStatus = wStatus::IOError("wServer::CleanEpoll, close() failed", strerror(errno));
    }
    mEpollFD = kFDUnknown;
    return CleanTaskPool();
}

wStatus wServer::AddToTaskPool(wTask* task) {
    mTaskPoolMutex->Lock();
    mTaskPool.push_back(task);
    mTaskPoolMutex->Unlock();
    return mStatus;
}

std::vector<wTask*>::iterator wServer::RemoveTaskPool(wTask* task) {
    mTaskPoolMutex->Lock();
    std::vector<wTask*>::iterator it = std::find(mTaskPool.begin(), mTaskPool.end(), task);
    if (it != mTaskPool.end()) {
    	SAFE_DELETE(*it);
        it = mTaskPool.erase(it);
    }
    mTaskPoolMutex->Unlock();
    return it;
}

wStatus wServer::CleanTaskPool() {
    mTaskPoolMutex->Lock();
    if (mTaskPool.size() > 0) {
	for (std::vector<wTask*>::iterator it = mTaskPool.begin(); it != mTaskPool.end(); it++) {
	    SAFE_DELETE(*it);
	}
    }
    mTaskPool.clear();
    mTaskPoolMutex->Unlock();
    return mStatus;
}

void wServer::CheckTimer(void* arg) {
    uint64_t interval = misc::GetTimeofday() - mLatestTm;
    if (interval < 100*1000) {
	return;
    }
    mLatestTm += interval;
    
    wServer* server = reinterpret_cast<wServer* >(arg);
    if (server->mHeartbeatTimer.CheckTimer(interval/1000)) {
	server->CheckTimeout();
    }
}

void wServer::CheckTimeout() {
    if (!mCheckSwitch) {
	return;
    }
    uint64_t nowTm = misc::GetTimeofday();
    
    //mTaskPoolMutex->Lock();
    if (mTaskPool.size() > 0) {
	for (std::vector<wTask*>::iterator iter = mTaskPool.begin(); iter != mTaskPool.end(); iter++) {
	    if ((*iter)->Socket()->ST() == kStConnect && (*iter)->Socket()->SS() == kSsConnected) {
		// 上一次发送时间间隔
		uint64_t interval = nowTm - (*iter)->Socket()->SendTm();
		if (interval >= kKeepAliveTm*1000) {
		    if (!(*iter)->HeartbeatSend().Ok() || (*iter)->HeartbeatOut()) {
			RemoveTask(task);
			// todo
			iter = mTaskPool.begin();
		    }
		}
	    }
	}
    }
    //mTaskPoolMutex->Unlock();
}

}	// namespace hnet