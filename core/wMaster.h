
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

class wEnv;
class wWorker;

class wMaster : private wNoncopyable {
public:
    wMaster();
    virtual ~wMaster();
    
    wStatus PrepareStart();
    wStatus SingleStart();		//单进程模式启动
    wStatus MasterStart();		//master-worker模式启动
    wStatus MasterExit();
    
    void WorkerStart(int n, int type = PROCESS_RESPAWN);
    pid_t SpawnWorker(string sTitle, int type = PROCESS_RESPAWN);
    void PassOpenChannel(struct ChannelReqOpen_t *pCh);
    void PassCloseChannel(struct ChannelReqClose_t *pCh);
    virtual wStatus HandleSignal();
    virtual wStatus NewWorker(const char* title, uint32_t slot, wWorker** ptr);

    // 如果有worker异常退出，则重启
    // 如果所有的worker都退出了，则返回0
    int ReapChildren();
	
    // 给所有worker进程发送信号
    void SignalWorker(int iSigno);

	/**
	 * master-worker工作模式主要做一下事情：
	 * 1. 设置进程标题 
	 * 2. 设置pid文件名
	 * 3. 设置启动worker个数
	 * 4. 设置自定义信号处理结构
	 * 5. 初始化服务器（创建、bind、listen套接字） 
	 */
	virtual wStatus PrepareRun() {}
	virtual wStatus Run() {}
	virtual wStatus ReloadMaster() {}

	// 注册信号回调
	// 可重写全局变量g_signals，实现自定义信号处理
	void InitSignals();

	// 回收退出进程状态（waitpid以防僵尸进程）
	// 更新进程表
	void ProcessGetStatus();
	wStatus CreatePidFile();
	wStatus DeletePidFile();

protected:
	friend class wWorker;
	wStatus mStatus;
	
	//MASTER_STATUS mStatus {MASTER_INIT};
	
	// 进程类别（master、worker、单进程）
	//int8_t mProcess;
	
	wEnv *mEnv;
	string mPidPath;
	
	//master相关
	//wFile mPidFile;	//pid文件
	
	uint8_t mNcpu;	// cpu个数
	pid_t mPid;		// master进程id
	
	// 进程表
	wWorker *mWorkerPool[kMaxPorcess];
	uint32_t mWorkerNum; // worker数量
	uint32_t mSlot;		// worker分配索引
	
	// 主进程master构建惊群锁（进程共享）
	uint32_t mDelay;	// 延时时间,默认500ms
};

}	// namespace hnet

#endif