
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_WORKER_H_
#define _W_WORKER_H_

#include <map>
#include <vector>
#include "wCore.h"
#include "wNoncopyable.h"
#include "wChannelSocket.h"

namespace hnet {

const uint32_t	kRlimitCore = 1024;
const char     	kWorkerTitle[] = " - worker process";

class wMaster;
class wServer;
class wChannelSocket;

class wWorker : public wNoncopyable {
public:
	wWorker(std::string title, uint32_t slot, wMaster* master);
	virtual ~wWorker();

	virtual int PrepareRun() {
		return 0;
	}

	virtual int Run() {
		return 0;
	}

	int PrepareStart();
	int Start();

	inline wMaster* Master() { return mMaster;}
	inline std::string& Title() { return mTitle;}
	inline pid_t& Pid() { return mPid;}
	inline int& Priority() { return mPriority;}
	inline int& Respawn() { return mRespawn;}
	inline int& Detached() { return mDetached;}
	inline int& Exited() { return mExited;}
	inline int& Exiting() { return mExiting;}
	inline int& Stat() { return mStat;}
	inline uint32_t& Timeline() { return mTimeline;}
	inline uint32_t& Slot() { return mSlot;}

	inline wChannelSocket* Channel() { return mChannel;}

	inline int& ChannelFD(uint8_t i) { return (*mChannel)[i];}
	
	inline int ChannelClose(uint8_t i) {
		if (close(ChannelFD(i)) == -1) {
			return -1;
		}
		ChannelFD(i) = kFDUnknown;
		return 0;
	}

protected:
	friend class wMaster;
	friend class wServer;

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
	uint32_t mTimeline;

	uint32_t mSlot;	// 进程表中索引
	wChannelSocket* mChannel;	// worker进程channel
};

}	// namespace hnet

#endif
