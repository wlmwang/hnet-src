
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include "wMultiClient.h"
#include "wEnv.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"
#include "wHttpTask.h"

namespace hnet {

wMultiClient::wMultiClient(wConfig* config, wServer* server) : mTick(0), mHeartbeatTurn(kHeartbeatTurn), mScheduleTurn(kScheduleTurn), mScheduleOk(true),
mEpollFD(kFDUnknown), mTimeout(10), mTask(NULL), mConfig(config), mServer(server) {
	assert(mConfig != NULL);
    mLatestTm = soft::TimeUsec();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wMultiClient::~wMultiClient() {
    CleanTask();
}

const wStatus& wMultiClient::AddConnect(int type, const std::string& ipaddr, uint16_t port, const std::string& protocol) {
    if (type >= kClientNumShard) {
        return mStatus = wStatus::Corruption("wMultiClient::AddConnect failed", "overload type");
    }
    
    wSocket *socket;
    if (protocol == "TCP") {
       SAFE_NEW(wTcpSocket(kStConnect), socket);
    } else if (protocol == "HTTP") {
        SAFE_NEW(wTcpSocket(kStConnect, kSpHttp), socket);
    } else if (protocol == "UNIX") {
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

    // 连接server
    int64_t ret;
    if (!(mStatus = socket->Connect(&ret, ipaddr, port)).Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }
    socket->SS() = kSsConnected;

    if (protocol == "TCP") {
        NewTcpTask(socket, &mTask, type);
    } else if (protocol == "HTTP") {
        NewHttpTask(socket, &mTask, type);
    } else if (protocol == "UNIX") {
        NewUnixTask(socket, &mTask, type);
    } else {
        mStatus = wStatus::NotSupported("wMultiClient::AcceptConn", "unknown task");
    }

    if (mStatus.Ok()) {
        if (!AddTask(mTask, EPOLLIN, EPOLL_CTL_ADD, true).Ok()) {
            RemoveTask(mTask, NULL, true);
        } else if (!(mStatus = mTask->Connect()).Ok()) {
            RemoveTask(mTask, NULL, true);
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

    // 连接server
    int64_t ret;
    if (!(mStatus = socket->Connect(&ret, socket->Host(), socket->Port())).Ok()) {
        return mStatus;
    }
    socket->SS() = kSsConnected;

    // 重置task缓冲
    task->ResetBuffer();
    if (!(mStatus = AddTask(mTask, EPOLLIN, EPOLL_CTL_ADD, false)).Ok()) {
        RemoveTask(mTask, NULL, false);
    } else if (!(mStatus = task->ReConnect()).Ok()) {
        RemoveTask(mTask, NULL, false);
    }
    return mStatus;
}

const wStatus& wMultiClient::DisConnect(wTask* task) {
    return RemoveTask(task);
}

const wStatus& wMultiClient::PrepareStart() {
    soft::TimeUpdate();

    if (!InitEpoll().Ok()) {
       return mStatus;
    }
    return PrepareRun();
}

const wStatus& wMultiClient::Start() {
    // 进入服务主服务
    while (true) {
        soft::TimeUpdate();

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
    return mStatus;
}

const wStatus& wMultiClient::NewUnixTask(wSocket* sock, wTask** ptr, int type) {
    SAFE_NEW(wUnixTask(sock, type), *ptr);
    if (*ptr == NULL) {
        return mStatus = wStatus::IOError("wMultiClient::NewUnixTask", "new failed");
    }
    return mStatus;
}

const wStatus& wMultiClient::NewHttpTask(wSocket* sock, wTask** ptr, int type) {
    SAFE_NEW(wHttpTask(sock, type), *ptr);
    if (*ptr == NULL) {
        return mStatus = wStatus::IOError("wMultiClient::wHttpTask", "new failed");
    }
    return mStatus;
}

const wStatus& wMultiClient::InitEpoll() {
    if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
       return mStatus = wStatus::IOError("wMultiClient::InitEpoll, epoll_create() failed", error::Strerror(errno));
    }
    return mStatus;
}

void wMultiClient::Locks(std::vector<int>* slot, std::vector<int>* blackslot) {
    if (slot) {
        for (std::vector<int>::iterator it = slot->begin(); it != slot->end(); it++) {
            if (blackslot && std::find(blackslot->begin(), blackslot->end(), *it) != blackslot->end()) {
                continue;
            } else if (*it < kClientNumShard) {
                mTaskPoolMutex[*it].Lock();
            }
        }
    } else {
        for (int i = 0; i < kClientNumShard; i++) {
            if (blackslot && std::find(blackslot->begin(), blackslot->end(), i) != blackslot->end()) {
                continue;
            }
            mTaskPoolMutex[i].Lock();
        }
    }
}

void wMultiClient::Unlocks(std::vector<int>* slot, std::vector<int>* blackslot) {
    if (slot) {
        for (std::vector<int>::iterator it = slot->begin(); it != slot->end(); it++) {
            if (blackslot && std::find(blackslot->begin(), blackslot->end(), *it) != blackslot->end()) {
                continue;
            } else if (*it < kClientNumShard) {
                mTaskPoolMutex[*it].Unlock();
            }
        }
    } else {
        for (int i = 0; i < kClientNumShard; i++) {
            if (blackslot && std::find(blackslot->begin(), blackslot->end(), i) != blackslot->end()) {
                continue;
            }
            mTaskPoolMutex[i].Unlock();
        }
    }
}

const wStatus& wMultiClient::Recv() {
    // 事件循环
    if (mScheduleTurn) Locks();
    struct epoll_event evt[kListenBacklog];
    int ret = epoll_wait(mEpollFD, evt, kListenBacklog, mTimeout);
    if (ret == -1) mStatus = wStatus::IOError("wMultiClient::Recv, epoll_wait() failed", error::Strerror(errno));
    if (mScheduleTurn) Unlocks();

    for (int i = 0; i < ret && evt[i].data.ptr; i++) {
        if (mScheduleTurn) Locks();
        wTask* task = reinterpret_cast<wTask*>(evt[i].data.ptr);
        std::vector<int> slot(1, task->Type());
        if (mScheduleTurn) Unlocks(NULL, &slot);

        if (task->Socket()->FD() == kFDUnknown || evt[i].events & (EPOLLERR|EPOLLPRI)) {
            task->Socket()->SS() = kSsUnconnect;
            RemoveTask(task, NULL, false);
        } else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
            if (evt[i].events & EPOLLIN) {  // 套接口准备好了读取操作
            	ssize_t size;
            	if (!(mStatus = task->TaskRecv(&size)).Ok()) {
                	task->Socket()->SS() = kSsUnconnect;
                    RemoveTask(task, NULL, false);
                }
            } else if (evt[i].events & EPOLLOUT) {
                if (task->SendLen() == 0) { // 清除写事件
                    AddTask(task, EPOLLIN, EPOLL_CTL_MOD, false);
                } else {
                    // 套接口准备好了写入操作
                    // 写入失败，半连接，对端读关闭
                	ssize_t size;
                	if (!(mStatus = task->TaskSend(&size)).Ok()) {
                    	task->Socket()->SS() = kSsUnconnect;
                        RemoveTask(task, NULL, false);
                    }
                }
            }
        }

        if (mScheduleTurn) Unlocks(&slot);
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

#ifdef _USE_PROTOBUF_
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
#endif

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

#ifdef _USE_PROTOBUF_
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
#endif

const wStatus& wMultiClient::AddTask(wTask* task, int ev, int op, bool addpool) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP;
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt) == -1) {
        return mStatus = wStatus::IOError("wMultiClient::AddTask, epoll_ctl() failed", error::Strerror(errno));
    }
    // 方便异步发送
    mTask->SetClient(this);
    // 方便worker进程间通信
    mTask->Server() = mServer;
    if (addpool) {
        AddToTaskPool(task);
    }
    return mStatus;
}

const wStatus& wMultiClient::AddToTaskPool(wTask* task) {
    mTaskPool[task->Type()].push_back(task);
    return mStatus;
}

const wStatus& wMultiClient::RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter, bool delpool) {
    struct epoll_event evt;
    evt.events = 0;
    evt.data.ptr = NULL;
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt) < 0) {
        mStatus = wStatus::IOError("wMultiClient::RemoveTask, epoll_ctl() failed", error::Strerror(errno));
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

const wStatus& wMultiClient::CleanTask() {
    if (close(mEpollFD) == -1) {
        mStatus = wStatus::IOError("wMultiClient::CleanTask, close() failed", error::Strerror(errno));
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
	mTick = soft::TimeUsec() - mLatestTm;
	if (mTick < 10*1000) {
		return;
	}
	mLatestTm += mTick;

	if (mScheduleTurn) {
		// 任务开关
		if (mScheduleMutex.TryLock() == 0) {
		    if (mScheduleOk == true) {
		    	mScheduleOk = false;
		    	wEnv::Default()->Schedule(wMultiClient::ScheduleRun, this);
		    }
		    mScheduleMutex.Unlock();
		}
	} else {
        if (mHeartbeatTimer.CheckTimer(mTick/1000)) {
            CheckHeartBeat();
        }
	}
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
    for (int i = 0; i < kClientNumShard; i++) {
        std::vector<int> slot(1, i);
        if (mScheduleTurn) Locks(&slot);
        if (mTaskPool[i].size() > 0) {
        	std::vector<wTask*>::iterator it = mTaskPool[i].begin();
        	while (it != mTaskPool[i].end()) {
        		if ((*it)->Socket()->ST() == kStConnect) {
        			if ((*it)->Socket()->SS() == kSsUnconnect) {
                    	// 重连服务器
                    	ReConnect(*it);
        			} else { // 心跳检测
                        (*it)->HeartbeatSend(); // 发送心跳
                        if ((*it)->HeartbeatOut()) {    // 心跳超限

                            (*it)->DisConnect();
                            (*it)->Socket()->SS() = kSsUnconnect;
                            
                            RemoveTask(*it, NULL, false);
                        }
        			}
        		}
        		it++;
        	}
        }
        if (mScheduleTurn) Unlocks(&slot);
    }
}

}   // namespace hnet
