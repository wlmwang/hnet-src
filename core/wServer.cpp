
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h> 
#include "wEnv.h"
#include "wServer.h"
#include "wConfig.h"
#include "wSem.h"
#include "wMaster.h"
#include "wWorker.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wChannelSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"
#include "wChannelTask.h"

namespace hnet {

wServer::wServer(wConfig* config) : mMaster(NULL), mWorker(NULL), mConfig(config), mExiting(false), mTick(0), mHeartbeatTurn(kHeartbeatTurn), mScheduleOk(true),
mEpollFD(kFDUnknown), mTimeout(10), mTask(NULL), mAcceptSem(NULL), mUseAcceptTurn(kAcceptTurn), mAcceptHeld(false), mAcceptDisabled(0) {
    mLatestTm = misc::GetTimeofday();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wServer::~wServer() {
    CleanTask();
    SAFE_DELETE(mAcceptSem);
}

const wStatus& wServer::PrepareStart(const std::string& ipaddr, uint16_t port, std::string protocol) {
    if (!AddListener(ipaddr, port, protocol).Ok()) {
		return mStatus;
    }
    return PrepareRun();
}

const wStatus& wServer::SingleStart(bool daemon) {
    if (!InitEpoll().Ok()) {
		return mStatus;
    } else if (!Listener2Epoll(true).Ok()) {
		return mStatus;
    }
    
    // 单进程关闭惊群锁
    mUseAcceptTurn = false;

    // 进入服务主循环
    while (daemon) {
    	if (mExiting) {
		    ProcessExit();
		    CleanTask();
		    exit(0);
    	}
		Recv();
		HandleSignal();
		Run();
		CheckTick();
    }
    return mStatus;
}

const wStatus& wServer::WorkerStart(bool daemon) {
    if (!InitEpoll().Ok()) {
		return mStatus;
    } else if (!Listener2Epoll(true).Ok()) {
		return mStatus;
    } else if (!Channel2Epoll(true).Ok()) {
    	return mStatus;
    }

    // 惊群锁
    if (mUseAcceptTurn == true && mMaster->WorkerNum() > 1) {
    	if (!(mStatus = wEnv::Default()->NewSem(NULL, &mAcceptSem)).Ok()) {
    		return mStatus;
    	}
    	if (!RemoveListener(false).Ok()) {
    		return mStatus;
    	}
    } else {
    	mUseAcceptTurn = false;
    }

    // 进入服务主循环
    while (daemon) {
    	if (mExiting) {
    	    ProcessExit();
		    CleanTask();
		    exit(0);
    	}
		Recv();
		HandleSignal();
		Run();
		CheckTick();
    }
    return mStatus;
}

const wStatus& wServer::HandleSignal() {
    if (g_terminate) {
		ProcessExit();
		CleanTask();
		exit(0);
    } else if (g_quit)	{
		// 优雅退出
		g_quit = 0;
		if (!mExiting) {
		    mExiting = true;
		}
    }
    return mStatus;
}

const wStatus& wServer::NewTcpTask(wSocket* sock, wTask** ptr) {
    SAFE_NEW(wTcpTask(sock, Shard(sock)), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wServer::NewTcpTask", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wServer::NewUnixTask(wSocket* sock, wTask** ptr) {
    SAFE_NEW(wUnixTask(sock, Shard(sock)), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wServer::NewUnixTask", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wServer::NewChannelTask(wSocket* sock, wTask** ptr) {
	SAFE_NEW(wChannelTask(sock, mMaster, Shard(sock)), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wServer::NewChannelTask", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wServer::Recv() {
	if (mUseAcceptTurn == true && mAcceptHeld == false) {
		if (!(mStatus = mAcceptSem->TryWait()).Ok()) {
			return mStatus;
		}
		Listener2Epoll(false);
		mAcceptHeld = true;
	}

	// 事件循环
	std::vector<struct epoll_event> evt(kListenBacklog);
	int ret = epoll_wait(mEpollFD, &evt[0], kListenBacklog, mTimeout);
	if (ret == -1) {
		mStatus = wStatus::IOError("wServer::Recv, epoll_wait() failed", "");
	}
	wTask* task;
	ssize_t size;
	for (int i = 0 ; i < ret ; i++) {
		task = reinterpret_cast<wTask *>(evt[i].data.ptr);
		// 加锁
		int type = task->Type();
		mTaskPoolMutex[type].Lock();
		if (task->Socket()->FD() == kFDUnknown) {
			RemoveTask(task);
		} else if (evt[i].events & (EPOLLERR | EPOLLPRI)) {
			RemoveTask(task);
		} else if (task->Socket()->ST() == kStListen && task->Socket()->SS() == kSsListened) {
			if (evt[i].events & EPOLLIN) {
				AcceptConn(task);
			} else {
				mStatus = wStatus::IOError("wServer::Recv, accept error", "listen socket error event");
			}
		} else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
			if (evt[i].events & EPOLLIN) {
				// 套接口准备好了读取操作
				if (!(mStatus = task->TaskRecv(&size)).Ok()) {
					RemoveTask(task);
				}
			} else if (evt[i].events & EPOLLOUT) {
				// 清除写事件
				if (task->SendLen() == 0) {
					AddTask(task, EPOLLIN, EPOLL_CTL_MOD, false);
				} else {
					// 套接口准备好了写入操作
					// 写入失败，半连接，对端读关闭
					if (!(mStatus = task->TaskSend(&size)).Ok()) {
						RemoveTask(task);
					}
				}
			}
		}
		// 解锁
		mTaskPoolMutex[type].Unlock();
	}

	if (mUseAcceptTurn == true && mAcceptHeld == true) {
		RemoveListener(false);
		mAcceptHeld = false;
		mAcceptSem->Post();
	}
    return mStatus;
}

const wStatus& wServer::AcceptConn(wTask *task) {
    if (task->Socket()->SP() == kSpUnix) {
		int64_t fd;
		struct sockaddr_un sockAddr;
		socklen_t sockAddrSize = sizeof(sockAddr);
		if (!(mStatus = task->Socket()->Accept(&fd, reinterpret_cast<struct sockaddr*>(&sockAddr), &sockAddrSize)).Ok()) {
		    return mStatus;
		}

		// unix socket
		wUnixSocket *socket;
		SAFE_NEW(wUnixSocket(kStConnect), socket);
		socket->FD() = fd;
		socket->Host() = sockAddr.sun_path;
		socket->Port() = 0;
		socket->SS() = kSsConnected;
		if (!(mStatus = socket->SetFL()).Ok()) {
		    return mStatus;
		}
		NewUnixTask(socket, &mTask);
    } else if(task->Socket()->SP() == kSpTcp) {
		int64_t fd;
		struct sockaddr_in sockAddr;
		socklen_t sockAddrSize = sizeof(sockAddr);	
		if (!(mStatus = task->Socket()->Accept(&fd, reinterpret_cast<struct sockaddr*>(&sockAddr), &sockAddrSize)).Ok()) {
		    return mStatus;
		}

		// tcp socket
		wTcpSocket *socket;
		SAFE_NEW(wTcpSocket(kStConnect), socket);
		socket->FD() = fd;
		socket->Host() = inet_ntoa(sockAddr.sin_addr);
		socket->Port() = sockAddr.sin_port;
		socket->SS() = kSsConnected;
		if (!(mStatus = socket->SetFL()).Ok()) {
		    return mStatus;
		}
		NewTcpTask(socket, &mTask);
    } else {
		mStatus = wStatus::NotSupported("wServer::AcceptConn", "unknown task");
    }
    
    if (mStatus.Ok()) {
    	// 登录
		if (!(mStatus = mTask->Login()).Ok()) {
		    SAFE_DELETE(mTask);
		} else if (!AddTask(mTask, EPOLLIN, EPOLL_CTL_ADD, true).Ok()) {
		    SAFE_DELETE(mTask);
		}
    }
    return mStatus;
}

const wStatus& wServer::Broadcast(char *cmd, int len) {
    for (int i = 0; i < kServerNumShard; i++) {
	    if (mTaskPool[i].size() > 0) {
			for (std::vector<wTask*>::iterator it = mTaskPool[i].begin(); it != mTaskPool[i].end(); it++) {
				if ((*it)->Socket()->ST() == kStConnect && (*it)->Socket()->SS() == kSsConnected && (*it)->Socket()->SP() != kSpChannel
				&& ((*it)->Socket()->SF() == kSfSend || (*it)->Socket()->SF() == kSfRvsd)) {
					Send(*it, cmd, len);
				}
			}
	    }
    }
    return mStatus;
}

const wStatus& wServer::Broadcast(const google::protobuf::Message* msg) {
    for (int i = 0; i < kServerNumShard; i++) {
	    if (mTaskPool[i].size() > 0) {
			for (std::vector<wTask*>::iterator it = mTaskPool[i].begin(); it != mTaskPool[i].end(); it++) {
				if ((*it)->Socket()->ST() == kStConnect && (*it)->Socket()->SS() == kSsConnected && (*it)->Socket()->SP() != kSpChannel
				&& ((*it)->Socket()->SF() == kSfSend || (*it)->Socket()->SF() == kSfRvsd)) {
					Send(*it, msg);
				}
			}
	    }
    }
    return mStatus;
}

const wStatus& wServer::NotifyWorker(char *cmd, int len, uint32_t solt, const std::vector<uint32_t>* blacksolt) {
	ssize_t ret;
	char buf[kPackageSize];
	if (solt == kMaxProcess) {
		// 广播消息
		for (uint32_t i = 0; i < kMaxProcess; i++) {
			if (mMaster->Worker(i)->mPid == -1 || mMaster->Worker(i)->ChannelFD(0) == kFDUnknown) {
				continue;
			} else if (blacksolt != NULL && std::find(blacksolt->begin(), blacksolt->end(), i) != blacksolt->end()) {
				continue;
			}

			/* TODO: EAGAIN */
			wTask::Assertbuf(buf, cmd, len);
			mStatus = mMaster->Worker(i)->Channel()->SendBytes(buf, sizeof(uint32_t) + sizeof(uint8_t) + len, &ret);
	    }
	} else {
		if (mMaster->Worker(solt)->mPid != -1 && mMaster->Worker(solt)->ChannelFD(0) != kFDUnknown) {

			/* TODO: EAGAIN */
			wTask::Assertbuf(buf, cmd, len);
			mStatus = mMaster->Worker(solt)->Channel()->SendBytes(buf, sizeof(uint32_t) + sizeof(uint8_t) + len, &ret);
		} else {
			mStatus = wStatus::Corruption("wServer::NotifyWorker failed 1", "worker channel is null");
		}
	}

    return mStatus;
}

const wStatus& wServer::NotifyWorker(const google::protobuf::Message* msg, uint32_t solt, const std::vector<uint32_t>* blacksolt) {
	ssize_t ret;
	char buf[kPackageSize];
	uint32_t len = sizeof(uint8_t) + sizeof(uint16_t) + msg->GetTypeName().size() + msg->ByteSize();
	if (solt == kMaxProcess) {
		for (uint32_t i = 0; i < kMaxProcess; i++) {
			if (mMaster->Worker(i)->mPid == -1 || mMaster->Worker(i)->ChannelFD(0) == kFDUnknown) {
				continue;
			} else if (blacksolt != NULL && std::find(blacksolt->begin(), blacksolt->end(), i) != blacksolt->end()) {
				continue;
			}

			/* TODO: EAGAIN */
			wTask::Assertbuf(buf, msg);
			mStatus = mMaster->Worker(i)->Channel()->SendBytes(buf, sizeof(uint32_t) + len, &ret);
	    }
	} else {
		if (mMaster->Worker(solt)->mPid != -1 && mMaster->Worker(solt)->ChannelFD(0) != kFDUnknown) {

			/* TODO: EAGAIN */
			wTask::Assertbuf(buf, msg);
			mStatus = mMaster->Worker(solt)->Channel()->SendBytes(buf, sizeof(uint32_t) + len, &ret);
		} else {
			mStatus = wStatus::Corruption("wServer::NotifyWorker failed 2", "worker channel is null");
		}
	}
    return mStatus;
}

const wStatus& wServer::Send(wTask *task, char *cmd, size_t len) {
	if ((mStatus = task->Send2Buf(cmd, len)).Ok()) {
	    return AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
	}
    return mStatus;
}

const wStatus& wServer::Send(wTask *task, const google::protobuf::Message* msg) {
	if ((mStatus = task->Send2Buf(msg)).Ok()) {
	    return AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
	}
    return mStatus;
}

const wStatus& wServer::AddListener(const std::string& ipaddr, uint16_t port, std::string protocol) {
    wSocket *socket;
    if (protocol == "TCP") {
		SAFE_NEW(wTcpSocket(kStListen), socket);
    } else if(protocol == "UNIX") {
		SAFE_NEW(wUnixSocket(kStListen), socket);
    } else {
    	socket = NULL;
    }
    
    if (socket != NULL) {
		if (!(mStatus = socket->Open()).Ok()) {
		    SAFE_DELETE(socket);
		    return mStatus;
		}
		if (!(mStatus = socket->Listen(ipaddr, port)).Ok()) {
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

const wStatus& wServer::InitEpoll() {
    if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wServer::InitEpoll, epoll_create() failed", err);
    }
    return mStatus;
}

const wStatus& wServer::Listener2Epoll(bool addpool) {
    for (std::vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) {
    	if (!addpool) {
    		bool oldtask = false;
        	for (std::vector<wTask*>::iterator im = mTaskPool[Shard(*it)].begin(); im != mTaskPool[Shard(*it)].end(); im++) {
        		if ((*im)->Socket() == *it) {
        			oldtask = true;
        			AddTask(*im, EPOLLIN, EPOLL_CTL_ADD, false);
        			break;
        		}
        	}
        	if (!oldtask) {
        		return mStatus = wStatus::Corruption("wServer::Listener2Epoll failed", "no task find");
        	} else {
        		continue;
        	}
    	}

    	wTask *task;
    	switch ((*it)->SP()) {
		case kSpTcp:
		    NewTcpTask(*it, &task);
		    break;
		case kSpUnix:
		    NewUnixTask(*it, &task);
		    break;
		default:
			task = NULL;
		    mStatus = wStatus::NotSupported("wServer::Listener2Epoll", "unknown task");
		}
		if (mStatus.Ok()) {
		    if (!AddTask(task, EPOLLIN, EPOLL_CTL_ADD, true).Ok()) {
				SAFE_DELETE(task);
		    }
		}
    }
    return mStatus;
}

const wStatus& wServer::RemoveListener(bool delpool) {
    for (std::vector<wSocket*>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) {
    	uint32_t id = Shard(*it);
    	for (std::vector<wTask*>::iterator im = mTaskPool[id].begin(); im != mTaskPool[id].end(); im++) {
    		if ((*im)->Socket() == *it) {
    			RemoveTask(*im, &im, delpool);
    			break;
    		}
    	}
    }
    return mStatus;
}

const wStatus& wServer::Channel2Epoll(bool addpool) {
	if (mMaster != NULL && mMaster->Worker(mMaster->mSlot) != NULL) {
		wChannelSocket *socket = mMaster->Worker(mMaster->mSlot)->Channel();
		if (socket != NULL) {
			wTask *task;
			NewChannelTask(socket, &task);
			if (mStatus.Ok()) {
				AddTask(task, EPOLLIN, EPOLL_CTL_ADD, addpool);
			}
		} else {
			mStatus = wStatus::Corruption("wServer::Channel2Epoll failed", "channel is null");
		}
	} else {
		mStatus = wStatus::Corruption("wServer::Channel2Epoll failed", "worker is null");
	}
    return mStatus;
}

const wStatus& wServer::AddTask(wTask* task, int ev, int op, bool addpool) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP | EPOLLET;
    evt.data.fd = task->Socket()->FD();
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wServer::AddTask, epoll_ctl() failed", err);
    }
    // 方便异步发送
    task->SetServer(this);
    if (addpool) {
    	AddToTaskPool(task);
    }
    return mStatus;
}

const wStatus& wServer::RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter, bool delpool) {
    struct epoll_event evt;
    evt.data.fd = task->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt) < 0) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wServer::RemoveTask, epoll_ctl() failed", err);
    }
    if (delpool) {
        std::vector<wTask*>::iterator it = RemoveTaskPool(task);
        if (iter != NULL) {
        	*iter = it;
        }
    }
    return mStatus;
}

const wStatus& wServer::CleanTask() {
    if (close(mEpollFD) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wServer::CleanTask, close() failed", err);
    }
    mEpollFD = kFDUnknown;
    for (int i = 0; i < kServerNumShard; i++) {
    	CleanTaskPool(mTaskPool[i]);
    }
    return mStatus;
}

const wStatus& wServer::AddToTaskPool(wTask* task) {
    mTaskPool[task->Type()].push_back(task);
    return mStatus;
}

std::vector<wTask*>::iterator wServer::RemoveTaskPool(wTask* task) {
	int32_t type =  task->Type();
    std::vector<wTask*>::iterator it = std::find(mTaskPool[type].begin(), mTaskPool[type].end(), task);
    if (it != mTaskPool[type].end()) {
    	SAFE_DELETE(*it);
        it = mTaskPool[type].erase(it);
    }
    return it;
}

const wStatus& wServer::CleanTaskPool(std::vector<wTask*> pool) {
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

void wServer::ScheduleRun(void* argv) {
    wServer* server = reinterpret_cast<wServer* >(argv);
	if (server->mScheduleMutex.Lock() == 0) {
	    if (server->mHeartbeatTurn && server->mHeartbeatTimer.CheckTimer(server->mTick/1000)) {
	    	server->CheckHeartBeat();
	    }
	    server->mScheduleOk = true;
	}
    server->mScheduleMutex.Unlock();
}

void wServer::CheckHeartBeat() {
    uint64_t tm = misc::GetTimeofday();
    for (int i = 0; i < kServerNumShard; i++) {
    	mTaskPoolMutex[i].Lock();
	    if (mTaskPool[i].size() > 0) {
	    	std::vector<wTask*>::iterator it = mTaskPool[i].begin();
	    	while (it != mTaskPool[i].end()) {
	    		if ((*it)->Socket()->ST() == kStConnect && ((*it)->Socket()->SP() == kSpTcp || (*it)->Socket()->SP() == kSpUnix)) {
	    			if ((*it)->Socket()->SS() == kSsUnconnect) {
	    				// 断线连接
	    				RemoveTask(*it, &it);
	    				continue;
	    			} else {
	    				// 心跳检测
						uint64_t interval = tm - (*it)->Socket()->SendTm();
						if (interval >= kKeepAliveTm*1000) {
							// 发送心跳
							(*it)->HeartbeatSend();
							if ((*it)->HeartbeatOut()) {
								// 心跳超限
								RemoveTask(*it, &it);
								continue;
							}
						}
	    			}
	    		}
	    		it++;
	    	}
	    }
    	mTaskPoolMutex[i].Unlock();
    }
}

}	// namespace hnet
