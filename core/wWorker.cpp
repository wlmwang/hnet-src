
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorker.h"
#include "wMisc.h"
#include "wSigSet.h"
#include "wMaster.h"
#include "wWorkerIpc.h"
#include "wEnv.h"
#include "wConfig.h"
#include "wServer.h"

namespace hnet {

wWorker::wWorker(std::string title, uint32_t slot, wMaster* master) : mMaster(master), mTitle(title), mPid(-1),
mPriority(0), mRlimitCore(kRlimitCore), mSlot(slot) {
	SAFE_NEW(wChannelSocket(), mChannel);
	SAFE_NEW(wWorkerIpc(this), mIpc);
}

wWorker::~wWorker() {
	SAFE_DELETE(mChannel);
}

wStatus wWorker::Prepare() {
	// 设置当前进程优先级。进程默认优先级为0
	// -20 -> 20 高 -> 低。只有root可提高优先级，即可减少priority值
	if (mSlot < kMaxProcess && mPriority != 0) {
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
    for (uint32_t n = 0; n < kMaxProcess; n++) {
    	if (n == mSlot || mMaster->Worker(n) == NULL || mMaster->Worker(n)->mPid == -1) {
    		continue;
    	} else if (close(mMaster->Worker(n)->FD(1)) == -1) {
    		return mStatus = wStatus::IOError("wWorker::Prepare, channel close() failed", strerror(errno));
    	}
    }

    // 关闭该进程worker进程的ch[0]描述符
    if (close(mMaster->Worker(mSlot)->FD(1)) == -1) {
    	return mStatus = wStatus::IOError("wWorker::Prepare, channel close() failed", strerror(errno));
    }

	mStatus = PrepareRun();
	if (!mStatus.Ok()) {
		return mStatus;
	}
	
    // 进程标题
    mStatus = mMaster->mConfig->Setproctitle(kWorkerTitle, mTitle.c_str());
	if (!mStatus.Ok()) {
		return mStatus;
	}

    // 开启worker ipc进程通信线程
	if (mIpc != NULL) {
		mStatus = mIpc->StartThread();
	} else {
		mStatus = wStatus::IOError("wWorker::Prepare, start thread failed", "new ipc failed");
	}
	return mStatus;
}

wStatus wWorker::Start() {
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
	return mStatus = mMaster->mServer->WorkerStart();
}

}	// namespace hnet
