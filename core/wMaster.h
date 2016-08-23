
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MASTER_H_
#define _W_MASTER_H_

#include <map>
#include <vector>

#include "wCore.h"
#include "wLog.h"
#include "wMisc.h"
#include "wSingleton.h"
#include "wFile.h"
#include "wShm.h"
#include "wShmtx.h"
#include "wSigSet.h"
#include "wSignal.h"
#include "wWorker.h"
#include "wChannelCmd.h"	//*channel*_t

class wWorker;
template <typename T>
class wMaster : public wSingleton<T>
{
	//TODO.
	string mWorkerProcessTitle = "worker process";
	public:
		wMaster();
		virtual ~wMaster();
		
		void PrepareStart();
		void SingleStart();		//单进程模式启动
		void MasterStart();		//master-worker模式启动
		void MasterExit();
		
		void WorkerStart(int n, int type = PROCESS_RESPAWN);
		pid_t SpawnWorker(string sTitle, int type = PROCESS_RESPAWN);
		void PassOpenChannel(struct ChannelReqOpen_t *pCh);
		void PassCloseChannel(struct ChannelReqClose_t *pCh);
		virtual wWorker* NewWorker(int iSlot = 0);
		virtual void HandleSignal();
		/**
		 *  如果有worker异常退出，则重启
		 *  如果所有的worker都退出了，则返回0
		 */
		int ReapChildren();
		/**
		 *  给所有worker进程发送信号
		 */
		void SignalWorker(int iSigno);

		/**
		 * master-worker工作模式主要做一下事情：
		 * 1. 设置进程标题 
		 * 2. 设置pid文件名
		 * 3. 设置启动worker个数
		 * 4. 设置自定义信号处理结构
		 * 5. 初始化服务器（创建、bind、listen套接字） 
		 */
		virtual void PrepareRun() {}
		virtual void Run() {}
		virtual void ReloadMaster() {}

		/**
		 *  注册信号回调
		 *  可重写全局变量g_signals，实现自定义信号处理
		 */
		void InitSignals();
		/**
		 *  回收退出进程状态（waitpid以防僵尸进程）
		 *  更新进程表
		 */
		void ProcessGetStatus();
		int CreatePidFile();
		void DeletePidFile();

	public:
		int mErr;
		MASTER_STATUS mStatus {MASTER_INIT};
		int mProcess {PROCESS_SINGLE};	//进程类别（master、worker、单进程）
		
		//master相关
		wFile mPidFile;	//pid文件
		int mNcpu {0};		//cpu个数
		pid_t mPid {0};		//master进程id
		
		//进程表
		wWorker **mWorkerPool {NULL};
		int mWorkerNum {0};	//worker总数量
		int mSlot {0};		//进程表分配到索引
		
		//主进程master构建惊群锁（进程共享）
		int mDelay {500};	//延时时间。默认500ms
		int mUseMutex {0};	//惊群锁标识
		int mMutexHeld {0};	//是否持有锁
		wShm *mShmAddr {NULL};	//共享内存
		wShmtx *mMutex {NULL};	//accept mutex
};

#include "wMaster.inl"

#endif
