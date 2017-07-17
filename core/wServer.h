
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
#include "wNoncopyable.h"
#include "wMutex.h"
#include "wEnv.h"
#include "wMisc.h"
#include "wSocket.h"
#include "wTimer.h"
#include "wConfig.h"
#include "wMaster.h"
#include "wAtomic.h"

#ifdef _USE_PROTOBUF_
#include <google/protobuf/message.h>
#endif

namespace hnet {

class wConfig;
class wTask;
class wMaster;
class wWorker;
class wFileLock;
class wShm;

// 服务基础类
class wServer : private wNoncopyable {
public:
    explicit wServer(wConfig* config);
    virtual ~wServer();

    int PrepareStart(const std::string& ipaddr, uint16_t port, const std::string& protocol = "TCP");

    // single模式启动服务
    int SingleStart(bool daemon = true);

    // master-worker多进程架构
    // PrepareMaster 需在master进程中调用
    // WorkerStart在worker进程提供服务
    int WorkerStart(bool daemon = true);

    // 创建惊群锁（master调用）
    int InitAcceptMutex();
    // 释放惊群锁（master调用）
    int ReleaseAcceptMutex(int pid);

    // 异步广播消息
    int Broadcast(char *cmd, int len);
#ifdef _USE_PROTOBUF_
    int Broadcast(const google::protobuf::Message* msg);
#endif

    // 同步广播消息至worker进程   blacksolt为黑名单
    int SyncWorker(char *cmd, int len, uint32_t solt = kMaxProcess, const std::vector<uint32_t>* blackslot = NULL);
#ifdef _USE_PROTOBUF_
    int SyncWorker(const google::protobuf::Message* msg, uint32_t solt = kMaxProcess, const std::vector<uint32_t>* blackslot = NULL);
#endif

    // 同步广播消息至worker进程   blacksolt为黑名单
    int AsyncWorker(char *cmd, int len, uint32_t solt = kMaxProcess, const std::vector<uint32_t>* blackslot = NULL);
#ifdef _USE_PROTOBUF_
    int AsyncWorker(const google::protobuf::Message* msg, uint32_t solt = kMaxProcess, const std::vector<uint32_t>* blackslot = NULL);
#endif

    // 异步发送消息
    int Send(wTask *task, char *cmd, size_t len);
#ifdef _USE_PROTOBUF_
    int Send(wTask *task, const google::protobuf::Message* msg);
#endif

    // 检查时钟周期tick
    void CheckTick();

    // 新建客户端
    virtual int NewTcpTask(wSocket* sock, wTask** ptr);
    virtual int NewUdpTask(wSocket* sock, wTask** ptr);
    virtual int NewUnixTask(wSocket* sock, wTask** ptr);
    virtual int NewChannelTask(wSocket* sock, wTask** ptr);
    virtual int NewHttpTask(wSocket* sock, wTask** ptr);

    virtual int PrepareRun() {
        return 0;
    }
    
    // 服务主循环逻辑，继承可以定制服务
    virtual int Run() {
        return 0;
    }
    
    virtual int HandleSignal();

    // 连接检测（心跳）
    virtual void CheckHeartBeat();
    
    // single|worker进程退出函数
    virtual void ProcessExit() { }

    template<typename T = wConfig*>
    inline T Config() { return reinterpret_cast<T>(mConfig);}

    // master
    template<typename T = wMaster*>
    inline T Master() { return reinterpret_cast<T>(mMaster);}

    // 当前worker进程
    template<typename T = wWorker*>
    inline T Worker() { return mMaster->Worker<T>();}

    int AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool addpool = true);
    int RemoveTask(wTask* task, std::vector<wTask*>::iterator* iter = NULL, bool delpool = true);
    int FindTaskBySocket(wTask** task, const wSocket* sock);
    
protected:
    friend class wMaster;
    friend class wWorker;
    
    // 事件读写主调函数
    int Recv();
    // accept接受连接
    int AcceptConn(wTask *task);

    int InitEpoll();
    int AddListener(const std::string& ipaddr, uint16_t port, const std::string& protocol = "TCP");

    // 添加本进程channel socket到epoll侦听读事件队列
    int Channel2Epoll(bool addpool = true);

    // 添加listen socket到epoll侦听事件队列
    int Listener2Epoll(bool addpool = true);
    int RemoveListener(bool delpool = true);

    int CleanTask();
    int CleanListenSock();
    int DeleteAcceptFile();

    int AddToTaskPool(wTask *task);
    std::vector<wTask*>::iterator RemoveTaskPool(wTask *task);
    int CleanTaskPool(std::vector<wTask*> pool);

    bool mExiting;

    // 服务器当前时间 微妙
    uint64_t mLatestTm;
    uint64_t mTick;

    // 心跳任务，强烈建议移动互联网环境下打开，而非依赖keepalive机制保活
    bool mHeartbeatTurn;
    // 心跳定时器
    wTimer mHeartbeatTimer;

    // 多listen socket监听服务描述符
    std::vector<wSocket*> mListenSock;

    int mEpollFD;
    int64_t mTimeout;

    // task|pool
    std::vector<wTask*> mTaskPool;
    
    // 惊群锁
    wShm *mShm;
    wAtomic<int>* mAcceptAtomic;
    wFileLock* mAcceptFL;

    bool mUseAcceptTurn;
    bool mAcceptHeld;
    int64_t mAcceptDisabled;

    wMaster* mMaster;	// 引用进程表
    wConfig* mConfig;
    wEnv* mEnv;
};

}	// namespace hnet

#endif
