
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MULTI_CLIENT_H_
#define _W_MULTI_CLIENT_H_

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

const int kNumShardBits = 4;
const int kNumShard = 1 << kNumShardBits;

class wEnv;
class wTask;

// 多类型客户端（类型为0-15）
// 多用于与服务端长连，守护监听服务端消息
class wMultiClient : private wNoncopyable {
public:
    wMultiClient();
    virtual ~wMultiClient();

    // 添加连接
    wStatus AddConnect(int type, std::string ipaddr, uint16_t port, std::string protocol = "TCP");
    
    // 断开连接
    wStatus DisConnect(wTask *task);
    
    // 异步广播消息
    // 当 type== kNumShard 广播所有类型下的所有客户端
    wStatus Broadcast(char *cmd, size_t len, int type = kNumShard);
    wStatus Send(wTask *task, char *cmd, size_t len);

    wStatus Prepare();
    wStatus Start();
    
    virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr, int type = 0);
    virtual wStatus NewUnixTask(wSocket* sock, wTask** ptr, int type = 0);

    virtual wStatus PrepareRun() {
        return mStatus;
    }

    virtual wStatus Run() {
        return mStatus;
    }
	
    virtual void CheckTimeout();

protected:
    wStatus Recv();
    wStatus InitEpoll();

    wStatus AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool newconn = true);
    wStatus RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL);
    wStatus CleanTask();
    
    wStatus AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    wStatus CleanTaskPool(std::vector<wTask*> pool);

    static void CheckTimer(void* arg);
   
    wStatus mStatus;
    wEnv *mEnv;
    
    // 心跳开关，默认关闭。强烈建议移动互联网环境下打开，而非依赖keepalive机制保活
    bool mCheckSwitch;

    // 服务器当前时间 微妙
    uint64_t mLatestTm;
    // 定时器
    wTimer mHeartbeatTimer;

    int32_t mEpollFD;
    uint64_t mTimeout;

    // task|pool
    wTask *mTask;
    std::vector<wTask*> mTaskPool[kNumShard];
    wMutex mTaskPoolMutex[kNumShard];
};

}	// namespace hnet

#endif