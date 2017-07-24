
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

wMultiClient::wMultiClient(wConfig* config, wServer* server, bool join) : wThread(join), mTick(0),
mHeartbeatTurn(kHeartbeatTurn),mEpollFD(kFDUnknown), mTimeout(10), mConfig(config), mServer(server) {
	assert(mConfig != NULL);
    mLatestTm = soft::TimeUsec();
    mHeartbeatTimer = wTimer(kKeepAliveTm);
}

wMultiClient::~wMultiClient() {
    CleanTask();
}

int wMultiClient::AddConnect(int type, const std::string& ipaddr, uint16_t port, const std::string& protocol) {
    if (type >= kClientNumShard) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect () failed", "type overload");
        return -1;
    }
    
    wSocket *socket = NULL;
    if (protocol == "TCP") {
        HNET_NEW(wTcpSocket(kStConnect), socket);
    } else if (protocol == "HTTP") {
        HNET_NEW(wTcpSocket(kStConnect, kSpHttp), socket);
    } else if (protocol == "UNIX") {
        HNET_NEW(wUnixSocket(kStConnect), socket);
    }

    if (!socket) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect new() failed", error::Strerror(errno).c_str());
        return -1;
    }

    int ret = socket->Open();
    if (ret == -1) {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect Open() failed", "");
        return ret;
    }

    // 连接server
    ret = socket->Connect(ipaddr, port);
    if (ret == -1) {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect Connect() failed", "");
        return ret;
    }
    socket->SS() = kSsConnected;

    ret = socket->SetNonblock();
    if (ret == -1) {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AcceptConn SetNonblock() failed", "");
        return ret;
    }
        
    wTask* ctask = NULL;
    if (protocol == "TCP") {
        if (NewTcpTask(socket, &ctask, type) == -1) {
            HNET_DELETE(socket);
            HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect NewTcpTask() failed", "");
            return -1;
        }
    } else if (protocol == "HTTP") {
        if (NewHttpTask(socket, &ctask, type) == -1) {
            HNET_DELETE(socket);
            HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect NewHttpTask() failed", "");
            return -1;
        }
    } else if (protocol == "UNIX") {
        if (NewUnixTask(socket, &ctask, type) == -1) {
            HNET_DELETE(socket);
            HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect NewUnixTask() failed", "");
            return -1;
        }
    } else {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect () failed", "unknown sp");
        return -1;
    }

    ret = AddTask(ctask);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect AddTask() failed", "");
        return RemoveTask(ctask);
    }

    ret = ctask->Connect();
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::AddConnect Connect() failed", "");
        return RemoveTask(ctask);
    }
    return 0;
}

int wMultiClient::ReConnect(wTask* task) {
    wSocket *socket = task->Socket();
    
    socket->Close();
    int ret = socket->Open();
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::ReConnect Open() failed", "");
        return ret;
    }

    // 连接server
    ret = socket->Connect(socket->Host(), socket->Port());
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::ReConnect Connect() failed", "");
        return ret;
    }
    socket->SS() = kSsConnected;

    task->ResetBuffer();    // 重置task缓冲
    ret = AddTask(task, EPOLLIN, EPOLL_CTL_ADD, false);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::ReConnect AddTask() failed", "");
        return RemoveTask(task, NULL, false);
    }

    ret = task->ReConnect();
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::ReConnect ReConnect() failed", "");
        return RemoveTask(task, NULL, false);
    }
    return 0;
}

int wMultiClient::DisConnect(wTask* task) {
    return RemoveTask(task);
}

int wMultiClient::PrepareStart() {
    soft::TimeUpdate();

    int ret = InitEpoll();
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::PrepareStart InitEpoll() failed", "");
        return ret;
    }

    ret = PrepareRun();
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::PrepareStart PrepareRun() failed", "");
    }
    return ret;
}

int wMultiClient::Start() {
    // 进入服务主服务
    while (true) {
        soft::TimeUpdate();

        Recv();
        Run();
        CheckTick();
    }
    return 0;
}

int wMultiClient::RunThread() {
	return Start();
}

int wMultiClient::NewTcpTask(wSocket* sock, wTask** ptr, int type) {
    HNET_NEW(wTcpTask(sock, type), *ptr);
    if (!*ptr) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::NewTcpTask new() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

int wMultiClient::NewUnixTask(wSocket* sock, wTask** ptr, int type) {
    HNET_NEW(wUnixTask(sock, type), *ptr);
    if (!*ptr) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::NewUnixTask new() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

int wMultiClient::NewHttpTask(wSocket* sock, wTask** ptr, int type) {
    HNET_NEW(wHttpTask(sock, type), *ptr);
    if (!*ptr) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::NewHttpTask new() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

int wMultiClient::InitEpoll() {
    int ret = epoll_create(kListenBacklog);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::InitEpoll epoll_create() failed", error::Strerror(errno).c_str());
        return ret;
    }
    mEpollFD = ret;
    return 0;
}

int wMultiClient::Recv() {
    // 事件循环
    struct epoll_event evt[kListenBacklog];
    int ret = epoll_wait(mEpollFD, evt, kListenBacklog, mTimeout);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::Recv epoll_wait() failed", error::Strerror(errno).c_str());
    }

    for (int i = 0; i < ret && evt[i].data.ptr; i++) {
        wTask* task = reinterpret_cast<wTask*>(evt[i].data.ptr);

        if (task->Socket()->FD() == kFDUnknown || evt[i].events & (EPOLLERR|EPOLLPRI)) {
            task->Socket()->SS() = kSsUnconnect;
            RemoveTask(task, NULL, false);
        } else if (task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected) {
            if (evt[i].events & EPOLLIN) {  // 套接口准备好了读取操作
            	ssize_t size;
            	if (task->TaskRecv(&size) == -1) {
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
                	if (task->TaskSend(&size) == -1) {
                    	task->Socket()->SS() = kSsUnconnect;
                        RemoveTask(task, NULL, false);
                    }
                }
            }
        }
    }
    return 0;
}

int wMultiClient::Broadcast(char *cmd, size_t len, int type) {
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
    return 0;
}

#ifdef _USE_PROTOBUF_
int wMultiClient::Broadcast(const google::protobuf::Message* msg, int type) {
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
    return 0;
}
#endif

int wMultiClient::Send(wTask *task, char *cmd, size_t len) {
    int ret = 0;
    if (task && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected
        && (task->Socket()->SF() == kSfSend || task->Socket()->SF() == kSfRvsd)) {
        ret = task->Send2Buf(cmd, len);
        if (ret == 0) {
        	ret = AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
        }
    }
    return ret;
}

#ifdef _USE_PROTOBUF_
int wMultiClient::Send(wTask *task, const google::protobuf::Message* msg) {
    int ret = 0;
    if (task && task->Socket()->ST() == kStConnect && task->Socket()->SS() == kSsConnected 
        && (task->Socket()->SF() == kSfSend || task->Socket()->SF() == kSfRvsd)) {
        ret = task->Send2Buf(msg);
        if (ret == 0) {
        	ret = AddTask(task, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false);
        }
    }
    return ret;
}
#endif

int wMultiClient::AddTask(wTask* task, int ev, int op, bool addpool) {    
    task->SetClient(this);      // 方便异步发送
    task->Server() = mServer;   // 方便worker进程间通信

    struct epoll_event evt;
    evt.events = ev | EPOLLERR | EPOLLHUP;
    evt.data.ptr = task;
    int ret = epoll_ctl(mEpollFD, op, task->Socket()->FD(), &evt);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wServer::AddTask epoll_ctl() failed", error::Strerror(errno).c_str());
        return ret;
    }
    
    if (addpool) {
        return AddToTaskPool(task);
    }
    return 0;
}

int wMultiClient::AddToTaskPool(wTask* task) {
    mTaskPool[task->Type()].push_back(task);
    return 0;
}

int wMultiClient::RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter, bool delpool) {
    struct epoll_event evt;
    evt.events = 0;
    evt.data.ptr = NULL;
    int ret = epoll_ctl(mEpollFD, EPOLL_CTL_DEL, task->Socket()->FD(), &evt);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::RemoveTask epoll_ctl() failed", error::Strerror(errno).c_str());
    }

    if (delpool) {
        std::vector<wTask*>::iterator it = RemoveTaskPool(task);
        if (iter != NULL) {
            *iter = it;
        }
    }
    return ret;
}

std::vector<wTask*>::iterator wMultiClient::RemoveTaskPool(wTask* task) {
	int32_t type = task->Type();
    std::vector<wTask*>::iterator it = std::find(mTaskPool[type].begin(), mTaskPool[type].end(), task);
    if (it != mTaskPool[type].end()) {
        HNET_DELETE(*it);
        it = mTaskPool[type].erase(it);
    }
    return it;
}

int wMultiClient::CleanTask() {
    for (int i = 0; i < kClientNumShard; i++) {
        CleanTaskPool(mTaskPool[i]);
    }

    int ret = close(mEpollFD);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wMultiClient::CleanTask close() failed", error::Strerror(errno).c_str());
    }

    mEpollFD = kFDUnknown;
    return ret;
}

int wMultiClient::CleanTaskPool(std::vector<wTask*> pool) {
    if (pool.size() > 0) {
        for (std::vector<wTask*>::iterator it = pool.begin(); it != pool.end(); it++) {
            HNET_DELETE(*it);
        }
    }
    pool.clear();
    return 0;
}

void wMultiClient::CheckTick() {
	mTick = soft::TimeUsec() - mLatestTm;
	if (mTick < 10*1000) {
		return;
	}
	mLatestTm += mTick;

    if (mHeartbeatTurn && mHeartbeatTimer.CheckTimer(mTick/1000)) {
        CheckHeartBeat();
    }
}

void wMultiClient::CheckHeartBeat() {
    for (int i = 0; i < kClientNumShard; i++) {
        if (mTaskPool[i].size() > 0) {
        	std::vector<wTask*>::iterator it = mTaskPool[i].begin();
        	while (it != mTaskPool[i].end()) {
        		if ((*it)->Socket()->ST() == kStConnect) {
        			if ((*it)->Socket()->SS() == kSsUnconnect) {
                    	// 重连服务器
                    	ReConnect(*it);
        			} else { 
                        // 心跳检测
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
    }
}

}   // namespace hnet
