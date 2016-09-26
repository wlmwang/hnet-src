
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMultiClient.h"
#include "wEnv.h"
#include "wClient.h"

wMultiClient::wMultiClient() : mEpollFD(kFDUnknown), mEnv(wEnv::Default()), mTimeout(10) {
    mLatestTm = misc::GetTimeofday();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wMultiClient::~wMultiClient() {
    CleanClient();
}

wStatus wMultiClient::Prepare() {
    if (!InitEpoll().Ok()) {
	return mStatus;
    }
    return mStatus = PrepareRun();
}

wStatus wMultiClient::Start(bool deamon) {
    // 进入服务主服务
    while (deamon) {
        Recv();
        Run();
        CheckTimer();
    }
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
	return mStatus = wStatus::IOError("wServer::Recv, epoll_wait() failed", strerror(errno));
    }
    
    for (wClient* client = NULL, wTask task = NULL , int i = 0 ; i < iRet ; i++) {
        client = reinterpret_cast<wClient*>(evt[i].data.ptr);
        task = client->Task();

        if (task->Socket()->FD() == kFDUnknown) {
            continue;
        } else if (evt[i].events & (EPOLLERR | EPOLLPRI)) {
            // 给重连做准备
            task->Socket()->FD() = kFDUnknown;
            continue;
        } else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
            if (evt[i].events & EPOLLIN) {
                // 套接口准备好了读取操作
                mStatus = task->TaskRecv();
                if (!mStatus.Ok()) {
                    // 给重连做准备
                    task->Socket()->FD() = kFDUnknown;
                }
            } else if (evt[i].events & EPOLLOUT) {
                // 清除写事件
                if (task->SendLen() == 0) {
                    AddTask(client->mType, client, EPOLLIN, EPOLL_CTL_MOD, false);
                    continue;
                }
                // 套接口准备好了写入操作
                // 写入失败，半连接，对端读关闭
                mStatus = task->TaskSend();
                if (!mStatus.Ok()) {
                    // 给重连做准备
                    task->Socket()->FD() = kFDUnknown;
                }
            }
        }
    }
    return mStatus;
}

wStatus wMultiClient::Broadcast(const char *cmd, int len, int32_t type) {
    std::vector<wClient*> cs;
    if (type == kReserveType) {
        for (std::map<int, vector<wClient*> >::iterator it = mClientPool.begin(); it != mClientPool.end(); it++) {
            for (std::vector<wClient*>::iterator ic = it->second.begin(); ic != it->second.end(), ic++) {
                cs.push_back(*ic);
            }
        }
    } else {
        std::map<int, vector<wClient*> >::iterator it = mClientPool.find(type);
        cs = it->second;
    }
    if (cs.size() > 0) {
        for (std::vector<wClient*>::iterator it = cs.begin(); it != cs.end(); it++) {
            Send(*it, cmd, len);
        }
    }
    return mStatus;
}

wStatus wMultiClient::Send(wClient *client, const char *cmd, int len) {
    wTask = client->Task();
    if (task != NULL && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected 
    	&& (pTask->Socket()->SF() == kSfSend || pTask->Socket()->SF() == kSfRvsd)) {
        mStatus = task->Send2Buf(cmd, len);
        if (mStatus.Ok()) {
            return AddClient(client->mType, client, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
        }
    } else {
	mStatus = wStatus::IOError("wMultiClient::Send, send error", "socket cannot send message");
    }
    return mStatus;
}

void wMultiClient::CheckTimer() {
    uint64_t interval = misc::GetTimeofday() - mLatestTm;
    if (interval < 100*1000) {
	return;
    }
    mLatestTm += interval;
    
    if (mHeartbeatTimer.CheckTimer(interval/1000)) {
        CheckTimeout();
    }
}

void wMultiClient::CheckTimeout() {
    for (std::map<int, vector<wClient*> >::iterator it = mClientPool.begin(); it != mClientPool.end(); it++) {
        for (std::vector<wClient*>::iterator ic = it->second.begin(); ic != it->second.end(), ic++) {
            if ((*ic)->Task()->Socket()->FD() == kFDUnknown) {
                // 重连
                std::string protocol = (*ic)->Task()->Socket()->SP() == kSpUnix? "UNIX": "TCP";
                MountClient((*ic)->mType, (*ic)->Task()->Socket()->Host(), (*ic)->Task()->Socket()->Port(), protocol);
                RemoveClient(*ic, &ic);
                ic--;
            } else {
                // 上一次发送时间间隔
                uint64_t interval = nowTm - (*ic)->Task()->Socket()->SendTm();
                if (interval >= kKeepAliveTm*1000) {
                    if (!(*ic)->Task()->HeartbeatSend().Ok() || (*ic)->Task()->HeartbeatOut()) {
                        RemoveClient(*ic, &ic);
                        ic--;
                    }
                }
            }
        }
    }
}

wStatus wMultiClient::MountClient(int32_t type, std::string ipaddr, uint16_t port, std::string protocol) {
    if (type == kReserveType) {
        return mStatus = wStatus::IOError("wMultiClient::MountClient failed", "reserve type");
    }
    wClient* client;
    SAFE_NEW(wClient(type), client);
    if (client == NULL) {
        return mStatus = wStatus::IOError("wMultiClient::MountClient", "new failed");
    }

    mStatus = client->Connect(ipaddr, port, protocol);
    if (!mStatus.Ok()) {
        SAFE_DELETE(client);
        return mStatus;
    }

    if (!AddClient(type, client, EPOLLIN, EPOLL_CTL_ADD, true).Ok()) {
        SAFE_DELETE(client);
    }
    return mStatus;
}

wStatus wMultiClient::UnMountClient(wClient *client) {
    return RemoveClient(client);
}

wStatus wMultiClient::RemoveClient(wClient* client, std::vector<wClient*>::iterator* iter) {
    struct epoll_event evt;
    evt.data.fd = client->Task()->Socket()->FD();
    if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, evt.data.fd, &evt) < 0) {
	return mStatus = wStatus::IOError("wMultiClient::RemoveClient, epoll_ctl() failed", strerror(errno));
    }
    std::vector<wClient*>::iterator it = RemoveClientPool(client->mType, client);
    iter != NULL && (*iter = it);
    return mStatus;
}

std::vector<wClient*>::iterator wMultiClient::RemoveClientPool(int32_t type, wClient* client) {
    std::map<int, vector<client*> >::iterator it = mClientPool.find(type);
	if (it == mClientPool.end()) {
            //todo
	}

    std::vector<wClient*> vClient = it->second;
    std::vector<client*>::iterator ic = std::find(vClient.begin(), vClient.end(), client);
    if (ic != vClient.end()) {
    	vClient.erase(ic);
        SAFE_DELETE(*ic);
        mClientPool.erase(it);
        mClientPool.insert(pair<int, std::vector<wClient*> >(type, vClient));
    }
    return ic;
}

wStatus wMultiClient::CleanClient() {
    if (close(mEpollFD) == -1) {
	return mStatus = wStatus::IOError("wMultiClient::CleanClient, close() failed", strerror(errno));
    }
    mEpollFD = kFDUnknown;
    CleanClientPool();
    return mStatus;
}

wStatus wMultiClient::CleanClientPool() {
    if (mClientPool.size() > 0) {
	for (std::vector<wClient*>::iterator it = mClientPool.begin(); it != mClientPool.end(); it++) {
	    SAFE_DELETE(*it);
	}
    }
    mClientPool.clear();
    return mStatus;
}

wStatus wMultiClient::AddClient(int32_t type, wClient* client, int ev, int op, bool newconn) {
    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP; // |EPOLLET
    evt.data.fd = client->Task()->Socket()->FD();
    evt.data.ptr = client;
    if (epoll_ctl(mEpollFD, op, evt.data.fd, &evt) == -1) {
	return mStatus = wStatus::IOError("wMultiClient::AddClient, epoll_ctl() failed", strerror(errno));
    }
    if (newconn) {
    	AddToClientPool(type, client);
    }
    return mStatus;
}

wStatus wMultiClient::AddToClientPool(int32_t type, wClient* client) {
    std::vector<client*> clients;
    std::map<int, vector<client*> >::iterator it = mClientPool.find(type);
    if (it != mClientPool.end()) {
        clients = it->second;
        mClientPool.erase(it);
    }
    clients.push_back(client);
    mClientPool.insert(pair<int32_t, vector<wClient*> >(type, clients));
    return mStatus;
}
