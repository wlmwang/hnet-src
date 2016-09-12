
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

namespace hnet {

const uint32_t	kRlimitCore = 65535;
const char*		kProcessTitle = "worker process";

class wMaster;
class wWorkerIpc;
class wChannelSocket;

class wWorker : public wNoncopyable {
public:
	wWorker(const char* title, uint32_t slot, wMaster* master);
	virtual ~wWorker();

	virtual wStatus PrepareRun() {
		return mStatus = wStatus::IOError("wWorker::PrepareRun, worker will be exit", "method should be inherit");
	}

	virtual wStatus Run() {
		return mStatus = wStatus::IOError("wWorker::Run, worker will be closed", "method should be inherit");
	}

	wStatus PrepareStart();
	wStatus Start(bool daemon = true);
	
protected:
	friend class wMaster;
	friend class wWorkerIpc;
	
	wStatus mStatus;
	int mPriority;	// 进程优先级
	int mRlimitCore;// 连接限制

	//pid_t mPid;
	//const char* mTitle;
	uint32_t mSlot;		// 进程表分配到索引
	wMaster *mMaster;
	wWorkerIpc *mIpc;	// worker通信 主要通过channel同步个fd，填充进程表
	wChannelSocket *mChannel;	// worker进程channel
};

}	// namespace hnet

#endif