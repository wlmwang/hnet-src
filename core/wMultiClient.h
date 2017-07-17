
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
#include "wNoncopyable.h"
#include "wMutex.h"
#include "wMisc.h"
#include "wSocket.h"
#include "wTimer.h"
#include "wThread.h"
#include "wConfig.h"
#include "wServer.h"

#ifdef _USE_PROTOBUF_
#include <google/protobuf/message.h>
#endif

namespace hnet {

const int kClientNumShardBits = 4;
const int kClientNumShard = 1 << kClientNumShardBits;

class wTask;

// 多类型客户端（类型为0-15）
// 多用于与服务端长连，守护监听服务端消息
class wMultiClient : public wThread {
public:
    wMultiClient(wConfig* config, wServer* server = NULL, bool join = false);
    virtual ~wMultiClient();

    // 添加连接
    int AddConnect(int type, const std::string& ipaddr, uint16_t port, const std::string& protocol = "TCP");

    // 重连
    int ReConnect(wTask* task);
    
    // 断开连接
    int DisConnect(wTask *task);
    
    // 异步广播消息
    // 当 type== kNumShard 广播所有类型下的所有客户端
    int Broadcast(char *cmd, size_t len, int type = kClientNumShard);
#ifdef _USE_PROTOBUF_
    int Broadcast(const google::protobuf::Message* msg, int type = kClientNumShard);
#endif

    int Send(wTask *task, char *cmd, size_t len);
#ifdef _USE_PROTOBUF_
    int Send(wTask *task, const google::protobuf::Message* msg);
#endif

    int PrepareStart();
    int Start();
    
    virtual int RunThread();

    virtual int PrepareRun() {
    	return 0;
    }

    virtual int Run() {
    	return 0;
    }

    // 检查时钟周期tick
    void CheckTick();

    virtual int NewTcpTask(wSocket* sock, wTask** ptr, int type = 0);
    virtual int NewUnixTask(wSocket* sock, wTask** ptr, int type = 0);
	virtual int NewHttpTask(wSocket* sock, wTask** ptr, int type = 0);

    virtual void CheckHeartBeat();

    template<typename T = wConfig*>
    inline T Config() { return reinterpret_cast<T>(mConfig);}

    template<typename T = wServer*>
    inline T Server() { return reinterpret_cast<T>(mServer);}

    int AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool addpool = true);
    
protected:
    int Recv();
    int InitEpoll();

    int RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL, bool delpool = true);
    int CleanTask();
    
    int AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    int CleanTaskPool(std::vector<wTask*> pool);

    // 服务器当前时间 微妙
    uint64_t mLatestTm;
    uint64_t mTick;

    // 心跳任务，强烈建议移动互联网环境下打开，而非依赖keepalive机制保活
    bool mHeartbeatTurn;
    // 心跳定时器
    wTimer mHeartbeatTimer;

    int mEpollFD;
    int64_t mTimeout;

    // task|pool
    std::vector<wTask*> mTaskPool[kClientNumShard];

    wConfig* mConfig;
    wServer* mServer;
};

}	// namespace hnet

#endif
