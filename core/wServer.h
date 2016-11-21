
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SERVER_H_
#define _W_SERVER_H_

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
#include "wConfig.h"

namespace hnet {

const int kServerNumShardBits = 8;
const int kServerNumShard = 1 << kServerNumShardBits;

class wConfig;
class wTask;
class wSem;
class wMaster;
class wWorker;

// 服务基础类
class wServer : private wNoncopyable {
public:
    explicit wServer(wConfig* config);
    virtual ~wServer();

    const wStatus& PrepareStart(const std::string& ipaddr, uint16_t port, std::string protocol = "TCP");

    // single模式启动服务
    const wStatus& SingleStart(bool daemon = true);

    // master-worker多进程架构
    // PrepareMaster 需在master进程中调用
    // WorkerStart在worker进程提供服务
    const wStatus& WorkerStart(bool daemon = true);
    
    // 异步广播消息
    const wStatus& Broadcast(char *cmd, int len);
    const wStatus& Broadcast(const google::protobuf::Message* msg);

    // 广播消息至worker进程   blacksolt为黑名单
    const wStatus& NotifyWorker(char *cmd, int len, uint32_t solt = kMaxProcess, const std::vector<uint32_t>* blacksolt = NULL);
    const wStatus& NotifyWorker(const google::protobuf::Message* msg, uint32_t solt = kMaxProcess, const std::vector<uint32_t>* blacksolt = NULL);

    // 异步发送消息
    const wStatus& Send(wTask *task, char *cmd, size_t len);
    const wStatus& Send(wTask *task, const google::protobuf::Message* msg);

    // 检查时钟周期tick
    void CheckTick();

    // 新建客户端
    virtual const wStatus& NewTcpTask(wSocket* sock, wTask** ptr);
    virtual const wStatus& NewUnixTask(wSocket* sock, wTask** ptr);
    virtual const wStatus& NewChannelTask(wSocket* sock, wTask** ptr);
    
    virtual const wStatus& PrepareRun() {
        return mStatus;
    }
    // 服务主循环逻辑，继承可以定制服务
    virtual const wStatus& Run() {
        return mStatus;
    }
    
    virtual const wStatus& HandleSignal();

    // 连接检测（心跳）
    virtual void CheckHeartBeat();
    
    // single|worker进程退出函数
    virtual void ProcessExit() { }

    inline wConfig*& Config() { return mConfig;}
    inline wMaster*& Master() { return mMaster;}
    inline wWorker*& Worker() { return mWorker;}

protected:
    friend class wMaster;
    friend class wWorker;
  
    // 散列到mTaskPool的某个分组中
    static uint32_t Shard(wSocket* sock) {
        uint32_t hash = misc::Hash(sock->Host().c_str(), sock->Host().size(), 0);
        return hash >> (32 - kServerNumShardBits);
    }

    // 事件读写主调函数
    const wStatus& Recv();
    // accept接受连接
    const wStatus& AcceptConn(wTask *task);

    const wStatus& InitEpoll();
    const wStatus& AddListener(const std::string& ipaddr, uint16_t port, std::string protocol = "TCP");

    // 添加channel socket到epoll侦听事件队列
    const wStatus& Channel2Epoll(bool addpool = true);

    // 添加listen socket到epoll侦听事件队列
    const wStatus& Listener2Epoll(bool addpool = true);
    const wStatus& RemoveListener(bool delpool = true);

    const wStatus& AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool addpool = true);
    const wStatus& RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL, bool delpool = true);
    const wStatus& CleanTask();

    const wStatus& AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    const wStatus& CleanTaskPool(std::vector<wTask*> pool);

    static void ScheduleRun(void* argv);

    wStatus mStatus;
    wMaster* mMaster;	// 引用进程表
    wWorker* mWorker;	// 当前worker进程
    wConfig* mConfig;
    bool mExiting;

    // 服务器当前时间 微妙
    uint64_t mLatestTm;
    uint64_t mTick;

    // 心跳任务，强烈建议移动互联网环境下打开，而非依赖keepalive机制保活
    bool mHeartbeatTurn;
    // 心跳定时器
    wTimer mHeartbeatTimer;

    bool mScheduleOk;
    wMutex mScheduleMutex;

    // 多listen socket监听服务描述符
    std::vector<wSocket*> mListenSock;

    int32_t mEpollFD;
    uint64_t mTimeout;

    // task|pool
    wTask *mTask;
    std::vector<wTask*> mTaskPool[kServerNumShard];
    wMutex mTaskPoolMutex[kServerNumShard];

    // 惊群锁
    wSem *mAcceptSem;
    bool mUseAcceptTurn;
    bool mAcceptHeld;
    int64_t mAcceptDisabled;
};

}	// namespace hnet

#endif
