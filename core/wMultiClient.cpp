
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

wMultiClient::wMultiClient() : mEnv(wEnv::Default()), mCheckSwitch(false), mEpollFD(kFDUnknown), mTimeout(10), mTask(NULL) {
    mLatestTm = misc::GetTimeofday();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wMultiClient::~wMultiClient() {
    CleanTask();
}

wStatus wMultiClient::AddConnect(int type, std::string ipaddr, uint16_t port, std::string protocol) {
    if (type >= kClientNumShard) {
        return mStatus = wStatus::IOError("wMultiClient::AddConnect failed", "overload type");
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

    mStatus = socket->Open();
    if (!mStatus.Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }

    int64_t ret;
    mStatus = socket->Connect(&ret, ipaddr, port);
    if (!mStatus.Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }
    socket->SS() = kSsConnected;

    if (protocol == "TCP") {
        mStatus = NewTcpTask(socket, &mTask, type);
    } else if(protocol == "UNIX") {
        mStatus = NewUnixTask(socket, &mTask, type);
    } else {
        mStatus = wStatus::IOError("wMultiClient::AcceptConn", "unknown task");
    }

    if (mStatus.Ok()) {
        // 登录
        if (!mTask->Login().Ok()) {
            SAFE_DELETE(mTask);
        } else if (!AddTask(mTask, EPOLLIN, EPOLL_CTL_ADD, true).Ok()) {
            SAFE_DELETE(mTask);
        } else {
        	mTask->SetClient(this);
        }
    }
    return mStatus;
}

wStatus wMultiClient::DisConnect(wTask* task) {
    return RemoveTask(task);
}

wStatus wMultiClient::PrepareStart() {
    if (!InitEpoll().Ok()) {
       return mStatus;
    }
    return mStatus = PrepareRun();
}

wStatus wMultiClient::Start() {
    
	// 添加心跳任务到线程池中
    if (mCheckSwitch) {
        mEnv->Schedule(wMultiClient::CheckTimer, this);
    }

    // 进入服务主服务
    while (true) {
        Recv();
        Run();
    }
}

wStatus wMultiClient::NewTcpTask(wSocket* sock, wTask** ptr, int type) {
    SAFE_NEW(wTcpTask(sock, type), *ptr);
    if (*ptr == NULL) {
        return wStatus::IOError("wMultiClient::NewTcpTask", "new failed");
    }
    return mStatus;
}

wStatus wMultiClient::NewUnixTask(wSocket* sock, wTask** ptr, int type) {
    SAFE_NEW(wUnixTask(sock, type), *ptr);
    if (*ptr == NULL) {
        return wStatus::IOError("wMultiClient::NewUnixTask", "new failed");
    }
    return mStatus;
}

wStatus wMultiClient::InitEpoll() {
    if ((mEpollFD = epoll_create(kListenBacklog)) == -1) {
       return mStatus = wStatus::IOError("wMultiClient::InitEpoll, epoll_create() failed", strerror(errno));
    }
    return mStatus;
}

wStatus wMultiClient::Recv() {
    std::vector<struct epoll_event> evt(kListenBacklog);
    int iRet = epoll_wait(mEpollFD, &evt[0], kListenBacklog, mTimeout);
    if (iRet == -1) {
       return mStatus = wStatus::IOError("wMultiClient::Recv, epoll_wait() failed", strerror(errno));
    }

    wTask* task;
    ssize_t size;
    for (int i = 0 ; i < iRet ; i++) {
        task = reinterpret_cast<wTask*>(evt[i].data.ptr);

        if (task->Socket()->FD() == kFDUnknown) {
            continue;
        } else if (evt[i].events & (EPOLLERR | EPOLLPRI)) {
            // 给重连做准备
            task->Socket()->FD() = kFDUnknown;
            continue;
        } else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
            if (evt[i].events & EPOLLIN) {
                // 套接口准备好了读取操作
                mStatus = task->TaskRecv(&size);
                if (!mStatus.Ok()) {
                    // 给重连做准备
                    task->Socket()->FD() = kFDUnknown;
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
                    // 给重连做准备
                    task->Socket()->FD() = kFDUnknown;
                }
            }
        }
    }
    return mStatus;
}

wStatus wMultiClient::Broadcast(char *cmd, size_t len, int type) {
    if (type == kClientNumShard) {
        for (int i = 0; i < kClientNumShard; i++) {
            mTaskPoolMutex[i].Lock();
            if (mTaskPool[i].size() > 0) {
                for (std::vector<wTask*>::iterator it = mTaskPool[i].begin(); it != mTaskPool[i].end(); it++) {
                    Send(*it, cmd, len);
                }
            }
            mTaskPoolMutex[i].Unlock();
        }
    } else {
        mTaskPoolMutex[type].Lock();
        if (mTaskPool[type].size() > 0) {
            for (std::vector<wTask*>::iterator it = mTaskPool[type].begin(); it != mTaskPool[type].end(); it++) {
                Send(*it, cmd, len);
            }
        }
        mTaskPoolMutex[type].Unlock();
    }
    return mStatus;
}

wStatus wMultiClient::Send(wTask *task, char *cmd, size_t len) {
    if (task != NULL && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected 
        && (task->Socket()->SF() == kSfSend || task->Socket()->SF() == kSfRvsd)) {
        mStatus = task->Send2Buf(cmd, len);
        if (mStatus.Ok()) {
            return AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
        }
    } else {
        mStatus = wStatus::IOError("wMultiClient::Send, send error", "socket cannot send message");
    }
    return mStatus;
}

void wMultiClient::CheckTimer(void* arg) {
    wMultiClient* client = reinterpret_cast<wMultiClient* >(arg);

    uint64_t interval = misc::GetTimeofday() - client->mLatestTm;
    if (interval < 100*1000) {
        return;
    }
    client->mLatestTm += interval;
    
    if (client->mHeartbeatTimer.CheckTimer(interval/1000)) {
        client->CheckTimeout();
    }

    // 重新将心跳任务添加到线程池中
    if (client->mCheckSwitch) {
    	client->mEnv->Schedule(wMultiClient::CheckTimer, client);
    }
}

void wMultiClient::CheckTimeout() {
    uint64_t nowTm = misc::GetTimeofday();
    
    for (int i = 0; i < kClientNumShard; i++) {
        mTaskPoolMutex[i].Lock();
        if (mTaskPool[i].size() > 0) {
            for (std::vector<wTask*>::iterator it = mTaskPool[i].begin(); it != mTaskPool[i].end(); it++) {
                if ((*it)->Socket()->ST() == kStConnect && (*it)->Socket()->SS() == kSsConnected) {
                    if ((*it)->Socket()->FD() == kFDUnknown) {
                        // 重连
                        std::string protocol = (*it)->Socket()->SP() == kSpUnix ? "UNIX": "TCP";
                        AddConnect((*it)->Type(), (*it)->Socket()->Host(), (*it)->Socket()->Port(), protocol);
                        RemoveTask(*it, &it);
                    } else {
                        // 上一次发送时间间隔
                        uint64_t interval = nowTm - (*it)->Socket()->SendTm();
                        if (interval >= kKeepAliveTm*1000) {
                            // 发送心跳
                            (*it)->HeartbeatSend();
                            // 心跳超限
                            if ((*it)->HeartbeatOut()) {
                                RemoveTask(*it, &it);
                            }
                        }
                    }
                }
            }
        }
        mTaskPoolMutex[i].Unlock();
    }
}

wStatus wMultiClient::RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter) {
    struct epoll_event evt;
    evt.data.fd = task->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt) < 0) {
        return mStatus = wStatus::IOError("wMultiClient::RemoveTask, epoll_ctl() failed", strerror(errno));
    }

    int32_t type = task->Type();
    mTaskPoolMutex[type].Lock();
    std::vector<wTask*>::iterator it = RemoveTaskPool(task);
    if (iter != NULL) {
        *iter = it;
    }
    mTaskPoolMutex[type].Unlock();
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

wStatus wMultiClient::AddTask(wTask* task, int ev, int op, bool newconn) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP | EPOLLET;
    evt.data.fd = task->Socket()->FD();
    evt.data.ptr = task;
    if (epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt) == -1) {
        return mStatus = wStatus::IOError("wMultiClient::AddTask, epoll_ctl() failed", strerror(errno));
    }

    if (newconn) {
        mTaskPoolMutex[task->Type()].Lock();
        AddToTaskPool(task);
        mTaskPoolMutex[task->Type()].Unlock();
    }
    return mStatus;
}

wStatus wMultiClient::AddToTaskPool(wTask* task) {
    mTaskPool[task->Type()].push_back(task);
    return mStatus;
}

wStatus wMultiClient::CleanTask() {
    if (close(mEpollFD) == -1) {
        return mStatus = wStatus::IOError("wMultiClient::CleanTask, close() failed", strerror(errno));
    }
    mEpollFD = kFDUnknown;

    for (int i = 0; i < kClientNumShard; i++) {
        mTaskPoolMutex[i].Lock();
        CleanTaskPool(mTaskPool[i]);
        mTaskPoolMutex[i].Unlock();
    }
    return mStatus;
}

wStatus wMultiClient::CleanTaskPool(std::vector<wTask*> pool) {
    if (pool.size() > 0) {
        for (std::vector<wTask*>::iterator it = pool.begin(); it != pool.end(); it++) {
            SAFE_DELETE(*it);
        }
    }
    pool.clear();
    return mStatus;
}

}   // namespace hnet
