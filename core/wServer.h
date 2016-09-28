
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SERVER_H_
#define _W_SERVER_H_

#include <algorithm>
#include <vector>
#include <sys/epoll.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wMutex.h"
#include "wMisc.h"
#include "wSocket.h"
#include "wTimer.h"

namespace hnet {

const int kNumShardBits = 8;
const int kNumShard = 1 << kNumShardBits;

class wEnv;
class wTask;
class wMaster;

// 服务基础类
class wServer : private wNoncopyable {
public:
    explicit wServer();
    virtual ~wServer();

    wStatus PrepareStart(std::string ipaddr, uint16_t port, std::string protocol = "TCP");

    // single模式启动服务
    wStatus SingleStart(bool daemon = true);

    // master-worker多进程架构
    // PrepareMaster 需在master进程中调用
    // WorkerStart在worker进程提供服务
    wStatus WorkerStart(bool daemon = true);
    
    // 异步发送消息
    wStatus Send(wTask *task, char *cmd, size_t len);
    // 异步广播消息
    wStatus Broadcast(char *cmd, int len);

    // 新建客户端
    virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr);
    virtual wStatus NewUnixTask(wSocket* sock, wTask** ptr);
    
    virtual wStatus PrepareRun() {
        return mStatus;
    }
    // 服务主循环逻辑，继承可以定制服务
    virtual wStatus Run() {
        return mStatus;
    }
    
    virtual wStatus HandleSignal();

    // 连接检测（心跳）
    virtual void CheckTimeout();
    
    // single|worker进程退出函数
    virtual void ProcessExit() { }

protected:
    friend class wMaster;
  
    // 散列到mTaskPool的某个分组中
    static uint32_t Shard(wSocket* sock) {
        uint32_t hash = misc::Hash(sock->Host().c_str(), sock->Host().size(), 0);
        return hash >> (32 - kNumShardBits);
    }

    // 事件读写主调函数
    wStatus Recv();
    // accept接受连接
    wStatus AcceptConn(wTask *task);

    wStatus InitEpoll();
    wStatus AddListener(std::string ipaddr, uint16_t port, std::string protocol = "TCP");
    // 添加到epoll侦听事件队列
    wStatus Listener2Epoll();
    wStatus CleanListener();
    
    wStatus AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool newconn = true);
    wStatus RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL);
    wStatus CleanTask();

    wStatus AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    wStatus CleanTaskPool(std::vector<wTask*> pool);

    static void CheckTimer(void* arg);

    wStatus mStatus;
    wEnv *mEnv;
    bool mExiting;

    // 心跳开关，默认关闭。强烈建议移动互联网环境下打开，而非依赖keepalive机制保活
    bool mCheckSwitch;
    // 服务器当前时间 微妙
    uint64_t mLatestTm;
    // 定时器
    wTimer mHeartbeatTimer;

    // 多监听服务
    std::vector<wSocket *> mListenSock;

    int32_t mEpollFD;
    uint64_t mTimeout;

    // task|pool
    wTask *mTask;
    std::vector<wTask*> mTaskPool[kNumShard];
    wMutex mTaskPoolMutex[kNumShard];
};

}	// namespace hnet

#endif