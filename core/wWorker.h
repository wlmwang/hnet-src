
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_WORKER_H_
#define _W_WORKER_H_

#include <map>
#include <vector>

#include "wCore.h"
#include "wLog.h"
#include "wMisc.h"
#include "wNoncopyable.h"
#include "wChannel.h"
#include "wWorkerIpc.h"

class wWorkerIpc;
class wWorker : public wNoncopyable
{
	public:
		wWorker(int iSlot = 0);
		virtual ~wWorker();
		/**
		 * 设置进程标题
		 */
		virtual void PrepareRun() {}
		virtual void Run() {}

		int OpenChannel();
		void CloseChannel();
		void PrepareStart(int iSlot, int iType, string sTitle, void **pData);
		void Start(bool bDaemon = true);
		
	public:
		int mErr;
		WORKER_STATUS mStatus {WORKER_INIT};
		pid_t mPid {-1};
		int mPriority {0};				//进程优先级
		int mRlimitCore {65535};		//连接限制
		
		string mName;		//进程名
		int mDetached {0};	//是否已分离
		int mExited {0};	//已退出 进程表mWorkerPool已回收
		int mExiting {0};	//正在退出
		int mStat {0};		//waitpid子进程退出状态
		int mRespawn {PROCESS_NORESPAWN};	//worker启动模式。退出是否重启
		int mJustSpawn {PROCESS_JUST_SPAWN};
		int mSlot {0};		//进程表分配到索引
		int mMutexHeld {0};

		//进程表
		wWorker **mWorkerPool {NULL};
		int mWorkerNum {0};	//worker总数量

		wChannel mCh;	//worker进程channel
		wWorkerIpc *mIpc {NULL};	//master-worker ipc
};

#endif
