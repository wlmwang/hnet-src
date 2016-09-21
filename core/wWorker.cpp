
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorker.h"
#include "wMisc.h"
#include "wSigSet.h"
#include "wWorkerIpc.h"
#include "wChannel.h"
#include "wShm.h"
#include "wShmtx.h"

namespace hnet {

wWorker::wWorker(const char* title, int slot, wMaster* master) : mTitle(title), mPid(-1), 
mPriority(0), mRlimitCore(kRlimitCore), mSlot(slot), mMaster(master) {
	SAFE_NEW(wWorkerIpc(this), mIpc);
	SAFE_NEW(wChannelSocket(), mChannel);
}

wWorker::~wWorker() {
	SAFE_DELETE(mIpc);
	SAFE_DELETE(mChannel);
}

wStatus wWorker::Prepare() {
	// 设置当前进程优先级。进程默认优先级为0
	// -20 -> 20 高 -> 低。只有root可提高优先级，即可减少priority值
	if (mSlot < kMaxPorcess && mPriority != 0) {
        if (setpriority(PRIO_PROCESS, 0, mPriority) == -1) {
			return mStatus = wStatus::IOError("wWorker::Prepare, setpriority() failed", strerror(errno));
        }
    }
	
	// 设置进程的最大文件描述符
    if (mRlimitCore > 0) {
		struct rlimit rlmt;
        rlmt.rlim_cur = static_cast<rlim_t>(mRlimitCore);
        rlmt.rlim_max = static_cast<rlim_t>(mRlimitCore);
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        	return mStatus = wStatus::IOError("wWorker::Prepare, setrlimit(RLIMIT_NOFILE) failed", strerror(errno));
        }
    }
	
	// 将其他进程的channel[1]关闭，自己的除外
    for (uint32_t n = 0; n < kMaxPorcess; n++) {
    	if (n == mSlot || mMaster->mWorkerPool[n] == NULL || mMaster->mWorkerPool[n]->mPid == -1) {
    		continue;
    	}
    	wWorker* worker = mMaster->mWorkerPool[n];
    	if (close((*worker->mChannel)[1]) == -1) {
    		return mStatus = wStatus::IOError("wWorker::Prepare, channel close() failed", strerror(errno));
    	}
    }

    // 关闭该进程worker进程的ch[0]描述符
    wWorker* worker = mMaster->mWorkerPool[mSlot];
    if (close((*worker->mChannel)[0]) == -1) {
    	return mStatus = wStatus::IOError("wWorker::Prepare, channel close() failed", strerror(errno));
    }

	mStatus = PrepareRun();
	if (!mStatus.Ok()) {
		return mStatus;
	}
	
    // 进程标题
    mStatus = mMaster->mConfig->mProcTitle->Setproctitle(kWorkerTitle, mTitle.c_str());
	if (!mStatus.Ok()) {
		return mStatus;
	}

    // 开启worker进程通信线程
	return mStatus = mIpc->StartThread();
}

void wWorker::Start() {
	// worker进程中不阻塞所有信号
	wSigSet ss;
	mStatus = ss.Procmask(SIG_SETMASK);
	if (!mStatus.Ok()) {
		return mStatus;
	}

    mStatus = Run();
    if (!mStatus.Ok()) {
    	return mStatus;
    }
	return mStatus = mServer->WorkerStart();
}

}	// namespace hnet
