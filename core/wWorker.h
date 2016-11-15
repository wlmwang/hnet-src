
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_WORKER_H_
#define _W_WORKER_H_

#include <map>
#include <vector>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wChannelSocket.h"

namespace hnet {

const uint32_t	kRlimitCore = 65535;
const char     	kWorkerTitle[] = " - worker process";

class wMaster;
class wServer;

class wWorker : public wNoncopyable {
public:
	wWorker(std::string title, uint32_t slot, wMaster* master);
	virtual ~wWorker();

	virtual wStatus PrepareRun() {
		return mStatus;
	}

	virtual wStatus Run() {
		return mStatus;
	}

	wStatus Prepare();
	wStatus Start();

	inline pid_t& Pid() { return mPid;}
	inline wMaster* Master() { return mMaster;}
    inline wChannelSocket* Channel() { return mChannel;}
    inline int& ChannelFD(uint8_t i) { return (*mChannel)[i];}

protected:
	friend class wMaster;
	friend class wServer;

	wStatus mStatus;
	wMaster *mMaster;	// 引用进程表
	std::string mTitle;	// 进程名
	pid_t mPid;
	int mPriority;	// 进程优先级
	int mRlimitCore;// 连接限制

	int mDetached;	// 是否已分离
	int mExited;	// 已退出 进程表mWorkerPool已回收
	int mExiting;	// 正在退出
	int mStat;		// waitpid子进程退出状态
	int mRespawn;	// worker启动模式。退出是否重启
	int mJustSpawn;

	uint32_t mSlot;	// 进程表中索引
	wChannelSocket* mChannel;	// worker进程channel
};

}	// namespace hnet

#endif
