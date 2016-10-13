
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MASTER_H_
#define _W_MASTER_H_

#include <map>
#include <vector>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

const char  kMasterTitle[] = " - master process";

class wEnv;
class wServer;
class wConfig;
class wWorker;
class wWorkerIpc;

class wMaster : private wNoncopyable {
public:
    wMaster(std::string title, wServer* server, wConfig* config);
    virtual ~wMaster();
    
    // 发送命令行信号
    wStatus SignalProcess(char* sig);

    // 准备启动
    wStatus PrepareStart();
    
    // 单进程模式启动
    wStatus SingleStart();

    // M-W模式启动(master-worker)
    wStatus MasterStart();

    // 修改pid文件名（默认hnet.pid）
    // 修改启动worker个数（默认cpu个数）
    // 修改自定义信号处理（默认定义在wSignal.cpp文件中）
    virtual wStatus PrepareRun() {
        return mStatus;
    }

    // 单进程为客户端事件循环体
    // M-W模式中为系统信号循环体
    virtual wStatus Run() {
        return mStatus;
    }
    
    virtual wStatus NewWorker(uint32_t slot, wWorker** ptr);
    virtual wStatus HandleSignal();
    
    virtual wStatus Reload();

    // master主进程退出函数
    virtual void ProcessExit() { }
    
    inline wWorker* Worker(uint32_t slot) {
        return mWorkerPool[slot];
    }

protected:
    friend class wWorker;
    friend class wWorkerIpc;

    // 启动n个worker进程
    wStatus WorkerStart(uint32_t n, int32_t type = kProcessRespawn);
    // 创建一个worker进程
    wStatus SpawnWorker(int64_t type);
    
    // 注册信号回调
    // 可覆盖全局变量g_signals，实现自定义信号处理
    wStatus InitSignals();

    // 如果有worker异常退出，则重启
    // 如果所有的worker都退出了，则 *live = 0
    wStatus ReapChildren(int8_t* live);

    wStatus CreatePidFile();
    wStatus DeletePidFile();
    
    // 给所有worker进程发送信号
    void SignalWorker(int signo);

    // 向所有已启动worker传递刚启动worker的channel描述符
    void PassOpenChannel(struct ChannelReqOpen_t *ch);
    // 向所有已启动worker传递关闭worker的channel消息
    void PassCloseChannel(struct ChannelReqClose_t *ch);

    // 回收退出进程状态（waitpid以防僵尸进程）
    void WorkerExitStat();

    wStatus mStatus;
    wEnv *mEnv;
    wServer* mServer;
    wConfig* mConfig;
    std::string mTitle;    // 进程名

    pid_t mPid;		// master进程id
    std::string mPidPath;
    uint8_t mNcpu;

    // 进程表
    uint32_t mSlot; // worker分配索引
    uint32_t mWorkerNum; // worker数量
    wWorker *mWorkerPool[kMaxProcess];
};

}	// namespace hnet

#endif
