
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMultiClient.h"
#include "wEnv.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"

namespace hnet {

wMultiClient::wMultiClient(wConfig* config, wServer* server) : mTick(0), mHeartbeatTurn(kHeartbeatTurn), mScheduleOk(true),
mEpollFD(kFDUnknown), mTimeout(10), mTask(NULL), mConfig(config), mServer(server) {
    mLatestTm = misc::GetTimeofday();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wMultiClient::~wMultiClient() {
    CleanTask();
}

const wStatus& wMultiClient::AddConnect(int type, const std::string& ipaddr, uint16_t port, std::string protocol) {
    if (type >= kClientNumShard) {
        return mStatus = wStatus::Corruption("wMultiClient::AddConnect failed", "overload type");
    }

    wSocket *socket;
    if (protocol == "TCP") {
       SAFE_NEW(wTcpSocket(kStConnect), socket);
    } else if(protocol == "UNIX") {
       SAFE_NEW(wUnixSocket(kStConnect), socket);
    } else {
        socket = NULL;
    }
    if (socket == NULL) {
        return mStatus = wStatus::IOError("wMultiClient::Connect", "socket new failed");
    }

    if (!(mStatus = socket->Open()).Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }

    int64_t ret;
    if (!(mStatus = socket->Connect(&ret, ipaddr, port)).Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }
    socket->SS() = kSsConnected;

    if (protocol == "TCP") {
        NewTcpTask(socket, &mTask, type);
    } else if(protocol == "UNIX") {
        NewUnixTask(socket, &mTask, type);
    } else {
        mStatus = wStatus::NotSupported("wMultiClient::AcceptConn", "unknown task");
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

const wStatus& wMultiClient::ReConnect(wTask* task) {
    wSocket *socket = task->Socket();
    if (socket->SS() == kSsConnected) {
    	socket->Close();
    }

    if (!(mStatus = socket->Open()).Ok()) {
        return mStatus;
    }

    int64_t ret;
    if (!(mStatus = socket->Connect(&ret, socket->Host(), socket->Port())).Ok()) {
        return mStatus;
    }
    socket->SS() = kSsConnected;
    task->ResetBuffer();
    return AddTask(mTask, EPOLLIN, EPOLL_CTL_ADD, false);
}

const wStatus& wMultiClient::DisConnect(wTask* task) {
    return RemoveTask(task);
}

const wStatus& wMultiClient::PrepareStart() {
    if (!InitEpoll().Ok()) {
       return mStatus;
    }
    return PrepareRun();
}

const wStatus& wMultiClient::Start() {
    // 进入服务主服务
    while (true) {
        Recv();
        Run();
        CheckTick();
    }
    return mStatus;
}

const wStatus& wMultiClient::RunThread() {
	return Start();
}

const wStatus& wMultiClient::NewTcpTask(wSocket* sock, wTask** ptr, int type) {
    SAFE_NEW(wTcpTask(sock, type), *ptr);
    if (*ptr == NULL) {
        return mStatus = wStatus::IOError("wMultiClient::NewTcpTask", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wMultiClient::NewUnixTask(wSocket* sock, wTask** ptr, int type) {
    SAFE_NEW(wUnixTask(sock, type), *ptr);
    if (*ptr == NULL) {
        return mStatus = wStatus::IOError("wMultiClient::NewUnixTask", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wMultiClient::InitEpoll() {
    if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
       return mStatus = wStatus::IOError("wMultiClient::InitEpoll, epoll_create() failed", strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wMultiClient::Recv() {
    std::vector<struct epoll_event> evt(kListenBacklog);
    int iRet = epoll_wait(mEpollFD, &evt[0], kListenBacklog, mTimeout);
    if (iRet == -1) {
       return mStatus = wStatus::IOError("wMultiClient::Recv, epoll_wait() failed", strerror(errno));
    }

    wTask* task;
    ssize_t size;
    for (int i = 0 ; i < iRet ; i++) {
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

const wStatus& wMultiClient::Broadcast(char *cmd, size_t len, int type) {
    if (type == kClientNumShard) {
        for (int i = 0; i < kClientNumShard; i++) {
            if (mTaskPool[i].size() > 0) {
                for (std::vector<wTask*>::iterator it = mTaskPool[i].begin(); it != mTaskPool[i].end(); it++) {
                    Send(*it, cmd, len);
                }
            }
        }
    } else {
        if (mTaskPool[type].size() > 0) {
            for (std::vector<wTask*>::iterator it = mTaskPool[type].begin(); it != mTaskPool[type].end(); it++) {
                Send(*it, cmd, len);
            }
        }
    }
    return mStatus;
}

const wStatus& wMultiClient::Broadcast(const google::protobuf::Message* msg, int type) {
    if (type == kClientNumShard) {
        for (int i = 0; i < kClientNumShard; i++) {
            if (mTaskPool[i].size() > 0) {
                for (std::vector<wTask*>::iterator it = mTaskPool[i].begin(); it != mTaskPool[i].end(); it++) {
                    Send(*it, msg);
                }
            }
        }
    } else {
        if (mTaskPool[type].size() > 0) {
            for (std::vector<wTask*>::iterator it = mTaskPool[type].begin(); it != mTaskPool[type].end(); it++) {
                Send(*it, msg);
            }
        }
    }
    return mStatus;
}

const wStatus& wMultiClient::Send(wTask *task, char *cmd, size_t len) {
    if (task != NULL && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected
        && (task->Socket()->SF() == kSfSend || task->Socket()->SF() == kSfRvsd)) {
        if ((mStatus = task->Send2Buf(cmd, len)).Ok()) {
        	AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
        }
    } else {
        mStatus = wStatus::IOError("wMultiClient::Send, send error", "socket cannot send message");
    }
    return mStatus;
}

const wStatus& wMultiClient::Send(wTask *task, const google::protobuf::Message* msg) {
    if (task != NULL && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected 
        && (task->Socket()->SF() == kSfSend || task->Socket()->SF() == kSfRvsd)) {
        if ((mStatus = task->Send2Buf(msg)).Ok()) {
        	AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
        }
    } else {
        mStatus = wStatus::Corruption("wMultiClient::Send, send error", "socket cannot send message");
    }
    return mStatus;
}

const wStatus& wMultiClient::NotifyWorker(char *cmd, int len, uint32_t solt, const std::vector<uint32_t>* blacksolt) {
	if (mServer) {
		mStatus = mServer->NotifyWorker(cmd, len, solt, blacksolt);
	} else {
		mStatus = wStatus::IOError("wMultiClient::NotifyWorker, notify error", "mServer is null");
	}
	return mStatus;
}

const wStatus& wMultiClient::NotifyWorker(const google::protobuf::Message* msg, uint32_t solt, const std::vector<uint32_t>* blacksolt) {
	if (mServer) {
		mStatus = mServer->NotifyWorker(msg, solt, blacksolt);
	} else {
		mStatus = wStatus::IOError("wMultiClient::NotifyWorker, notify error", "mServer is null");
	}
	return mStatus;
}

const wStatus& wMultiClient::RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter, bool delpool) {
    struct epoll_event evt;
    evt.data.fd = task->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt) < 0) {
        mStatus = wStatus::IOError("wMultiClient::RemoveTask, epoll_ctl() failed", strerror(errno));
    }
    if (delpool) {
        std::vector<wTask*>::iterator it = RemoveTaskPool(task);
        if (iter != NULL) {
            *iter = it;
        }
    }
    return mStatus;
}

std::vector<wTask*>::iterator wMultiClient::RemoveTaskPool(wTask* task) {
	int32_t type = task->Type();
    std::vector<wTask*>::iterator it = std::find(mTaskPool[type].begin(), mTaskPool[type].end(), task);
    if (it != mTaskPool[type].end()) {
        SAFE_DELETE(*it);
        it = mTaskPool[type].erase(it);
    }
    return it;
}

const wStatus& wMultiClient::AddTask(wTask* task, int ev, int op, bool addpool) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP | EPOLLET;
    evt.data.fd = task->Socket()->FD();
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt) == -1) {
        return mStatus = wStatus::IOError("wMultiClient::AddTask, epoll_ctl() failed", strerror(errno));
    }
    mTask->SetClient(this);
    if (addpool) {
        AddToTaskPool(task);
    }
    return mStatus;
}

const wStatus& wMultiClient::AddToTaskPool(wTask* task) {
    mTaskPool[task->Type()].push_back(task);
    return mStatus;
}

const wStatus& wMultiClient::CleanTask() {
    if (close(mEpollFD) == -1) {
        mStatus = wStatus::IOError("wMultiClient::CleanTask, close() failed", strerror(errno));
    }
    mEpollFD = kFDUnknown;
    for (int i = 0; i < kClientNumShard; i++) {
        CleanTaskPool(mTaskPool[i]);
    }
    return mStatus;
}

const wStatus& wMultiClient::CleanTaskPool(std::vector<wTask*> pool) {
    if (pool.size() > 0) {
        for (std::vector<wTask*>::iterator it = pool.begin(); it != pool.end(); it++) {
            SAFE_DELETE(*it);
        }
    }
    pool.clear();
    return mStatus;
}

void wMultiClient::CheckTick() {
	if (mScheduleMutex.TryLock() == 0) {
		mTick = misc::GetTimeofday() - mLatestTm;
		if (mTick >= 10*1000) {
		    mLatestTm += mTick;
		    // 添加任务到线程池中
		    if (mScheduleOk == true) {
		    	mScheduleOk = false;
				wEnv::Default()->Schedule(wMultiClient::ScheduleRun, this);
		    }
		}
	}
	mScheduleMutex.Unlock();
}

void wMultiClient::ScheduleRun(void* argv) {
    wMultiClient* client = reinterpret_cast<wMultiClient* >(argv);
	if (client->mScheduleMutex.Lock() == 0) {
	    if (client->mHeartbeatTurn && client->mHeartbeatTimer.CheckTimer(client->mTick/1000)) {
	    	client->CheckHeartBeat();
	    }
	    client->mScheduleOk = true;
	}
    client->mScheduleMutex.Unlock();
}

void wMultiClient::CheckHeartBeat() {
    uint64_t nowTm = misc::GetTimeofday();
    for (int i = 0; i < kClientNumShard; i++) {
        mTaskPoolMutex[i].Lock();
        if (mTaskPool[i].size() > 0) {
        	std::vector<wTask*>::iterator it = mTaskPool[i].begin();
        	while (it != mTaskPool[i].end()) {
        		if ((*it)->Socket()->ST() == kStConnect) {
        			if ((*it)->Socket()->SS() == kSsUnconnect) {
                    	// 重连服务器
                    	ReConnect(*it);
        			} else {
        				// 心跳检测
        				uint64_t interval = nowTm - (*it)->Socket()->SendTm();
        				if (interval >= kKeepAliveTm*1000) {
        					// 发送心跳
        					(*it)->HeartbeatSend();
        					if ((*it)->HeartbeatOut()) {
        						// 心跳超限
        						(*it)->Socket()->SS() = kSsUnconnect;
        						RemoveTask(*it, NULL, false);
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

}   // namespace hnet
