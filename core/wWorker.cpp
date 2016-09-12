
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorker.h"
#include "wLog.h"
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

wStatus wWorker::PrepareStart() {
	// 设置当前进程优先级。进程默认优先级为0
	// -20 -> 20 高 -> 低。只有root可提高优先级，即可减少priority值
	if (mSlot < kMaxPorcess && mPriority != 0) {
        if (setpriority(PRIO_PROCESS, 0, mPriority) == -1) {
			//return mStatus = wStatus::IOError("wWorker::PrepareStart, setpriority() failed", strerror(errno));
			exit(2);
        }
    }
	
	// 设置进程的最大文件描述符
    if (mRlimitCore > 0) {
		struct rlimit rlmt;
        rlmt.rlim_cur = static_cast<rlim_t>(mRlimitCore);
        rlmt.rlim_max = static_cast<rlim_t>(mRlimitCore);
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        	//return mStatus = wStatus::IOError("wWorker::PrepareStart, setrlimit(RLIMIT_NOFILE) failed", strerror(errno));
        	exit(2);
        }
    }
	
	// 将其他进程的channel[1]关闭，自己的除外
    for (uint32_t n = 0; n < wMaster->mWorkerNum; n++) {
        if (n == mSlot || wMaster->mWorkerPool[n] == NULL || wMaster->mWorkerPool[n]->mPid == -1 || (*wMaster->mWorkerPool[n]->mChannel)[1] == kFDUnknown) {
            continue;
        } else if (close((*wMaster->mWorkerPool[n]->mChannel)[1]) == -1) {
        	//return mStatus = wStatus::IOError("wWorker::PrepareStart, channel close() failed", strerror(errno));
        	exit(2);
        }
    }

    // 关闭该进程worker进程的ch[0]描述符
    if (close((*wMaster->mWorkerPool[mSlot]->mChannel)[0]) == -1) {
    	//return mStatus = wStatus::IOError("wWorker::PrepareStart, channel close() failed", strerror(errno));
    	exit(2);
    }
	
	// worker进程中不阻塞所有信号
	wSigSet sst;
	mStatus = sst.Procmask(SIG_SETMASK);
	if (!mStatus.Ok()) {
		//return mStatus;
		exit(2);
	}
	if (!PrepareRun().Ok()) {
		//return mStatus;
		exit(2);
	}
	return mStatus;
}

void wWorker::Start(bool daemon) {
	// 开启worker进程通信线程
	mIpc->StartThread();
	
	if (!Run.Ok()) {
		exit(2);
	}
	return mStatus;
}

}	// namespace hnet
