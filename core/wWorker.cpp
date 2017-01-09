
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorker.h"
#include "wMaster.h"
#include "wWorkerIpc.h"
#include "wMisc.h"
#include "wSigSet.h"
#include "wConfig.h"
#include "wServer.h"
#include "wChannelSocket.h"
#include "wChannelTask.h"

namespace hnet {

// 散列
uint32_t wWorker::Shard(wSocket* sock) {
	return wWorkerIpc::Shard(sock);
}

wWorker::wWorker(const std::string& title, uint32_t slot, wMaster* master) : mMaster(master), mTitle(title), mPid(-1),
mPriority(0), mRlimitCore(kRlimitCore), mSlot(slot) {
	assert(mMaster != NULL);
	SAFE_NEW(wChannelSocket(kStConnect), mChannel);
	SAFE_NEW(wWorkerIpc(this), mWorkerIpc);

	// 便于wMaster引用wWorker（当前进程）
	mMaster->mWorker = this;
}

wWorker::~wWorker() {
	SAFE_DELETE(mChannel);
}

const wStatus& wWorker::NewChannelTask(wSocket* sock, wTask** ptr) {
	SAFE_NEW(wChannelTask(sock, mMaster, Shard(sock)), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wWorker::NewChannelTask", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wWorker::PrepareStart() {
	// 设置当前进程优先级。进程默认优先级为0
	// -20 -> 20  高 -> 低。只有root可提高优先级，即可减少priority值
	if (mSlot < kMaxProcess && mPriority != 0) {
        if (setpriority(PRIO_PROCESS, 0, mPriority) == -1) {
			return mStatus = wStatus::IOError("wWorker::PrepareStart, setpriority(PRIO_PROCESS) failed", error::Strerror(errno));
        }
    }
	
	// 设置进程的最大文件描述符
    if (mRlimitCore > 0) {
		struct rlimit rlmt;
        rlmt.rlim_cur = static_cast<rlim_t>(mRlimitCore);
        rlmt.rlim_max = static_cast<rlim_t>(mRlimitCore);
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        	return mStatus = wStatus::IOError("wWorker::PrepareStart, setrlimit(RLIMIT_NOFILE) failed", error::Strerror(errno));
        }
    }
	
    // 只保留自身worker进程的channel[1]与其他所有worker进程的channel[0]描述符，即：本worker进程 [1]描述符 接收其他worker进程写入该全双工描述符[0]
    // [1] [0] [0] ...
    for (uint32_t n = 0; n < kMaxProcess; n++) {
    	// 将其他进程的channel[1]关闭，自己的除外
    	if (n == mSlot || mMaster->Worker(n) == NULL || mMaster->Worker(n)->mPid == -1) {
    		continue;
    	} else if (close(mMaster->Worker(n)->ChannelFD(1)) == -1) {
    		return mStatus = wStatus::IOError("wWorker::PrepareStart, channel close() failed", error::Strerror(errno));
    	}
    }
    // 关闭该进程worker进程的channel[0]描述符
    if (close(mMaster->Worker(mSlot)->ChannelFD(0)) == -1) {
    	return mStatus = wStatus::IOError("wWorker::PrepareStart, channel close() failed", error::Strerror(errno));
    }

	if (!(mStatus = PrepareRun()).Ok()) {
		return mStatus;
	}
	
    // 进程标题
	if (!(mStatus = mMaster->Server()->Config()->Setproctitle(kWorkerTitle, mTitle.c_str())).Ok()) {
		return mStatus;
	}

	// workerIpc进程通信线程预启动
	if (!(mStatus = mWorkerIpc->PrepareStart()).Ok()) {
		return mStatus;
	}

	// 便于wMaster引用wWorker（当前进程）
	mMaster->mWorker = this;
	return mStatus.Clear();
}

const wStatus& wWorker::Start() {
	// worker进程中恢复所有信号处理器
	wSigSet ss;
	if (!(mStatus = ss.Procmask(SIG_SETMASK)).Ok()) {
		return mStatus;
	}

    if (!(mStatus = Run()).Ok()) {
    	return mStatus;
    }

    // 启动workerIpc线程
	if (!(mStatus = mWorkerIpc->StartThread()).Ok()) {
		return mStatus;
	}

    // 启动worker服务器
	return mStatus = mMaster->Server()->WorkerStart();
}

}	// namespace hnet
