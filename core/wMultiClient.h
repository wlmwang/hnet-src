
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MULTI_CLIENT_H_
#define _W_MULTI_CLIENT_H_

#include <google/protobuf/message.h>
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
#include "wThread.h"

namespace hnet {

const int kClientNumShardBits = 4;
const int kClientNumShard = 1 << kClientNumShardBits;

class wTask;
class wConfig;

// 多类型客户端（类型为0-15）
// 多用于与服务端长连，守护监听服务端消息
class wMultiClient : public wThread {
public:
    wMultiClient(wConfig* config);
    virtual ~wMultiClient();

    // 添加连接
    const wStatus& AddConnect(int type, const std::string& ipaddr, uint16_t port, std::string protocol = "TCP");

    // 重连
    const wStatus& ReConnect(wTask* task);
    
    // 断开连接
    const wStatus& DisConnect(wTask *task);
    
    // 异步广播消息
    // 当 type== kNumShard 广播所有类型下的所有客户端
    const wStatus& Broadcast(char *cmd, size_t len, int type = kClientNumShard);
    const wStatus& Broadcast(const google::protobuf::Message* msg, int type = kClientNumShard);
    const wStatus& Send(wTask *task, char *cmd, size_t len);
    const wStatus& Send(wTask *task, const google::protobuf::Message* msg);

    const wStatus& PrepareStart();
    const wStatus& Start();
    
    virtual const wStatus& RunThread();

    virtual const wStatus& PrepareRun() {
    	return mStatus;
    }

    virtual const wStatus& Run() {
    	return mStatus;
    }

    // 检查时钟周期tick
    void CheckTick();

    virtual const wStatus& NewTcpTask(wSocket* sock, wTask** ptr, int type = 0);
    virtual const wStatus& NewUnixTask(wSocket* sock, wTask** ptr, int type = 0);
	
    virtual void CheckHeartBeat();

protected:
    const wStatus& Recv();
    const wStatus& InitEpoll();

    const wStatus& AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool addpool = true);
    const wStatus& RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL, bool delpool = true);
    const wStatus& CleanTask();
    
    const wStatus& AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    const wStatus& CleanTaskPool(std::vector<wTask*> pool);

    static void ScheduleRun(void* argv);

    // 服务器当前时间 微妙
    uint64_t mLatestTm;
    uint64_t mTick;

    // 心跳任务，强烈建议移动互联网环境下打开，而非依赖keepalive机制保活
    bool mHeartbeatTurn;
    // 心跳定时器
    wTimer mHeartbeatTimer;

    bool mScheduleOk;
    wMutex mScheduleMutex;

    int32_t mEpollFD;
    uint64_t mTimeout;

    // task|pool
    wTask *mTask;
    std::vector<wTask*> mTaskPool[kClientNumShard];
    wMutex mTaskPoolMutex[kClientNumShard];
    wConfig* mConfig;

    wStatus mStatus;
};

}	// namespace hnet

#endif
