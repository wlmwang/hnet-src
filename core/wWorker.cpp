
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorker.h"
#include "wMisc.h"
#include "wLogger.h"
#include "wSigSet.h"
#include "wMaster.h"
#include "wConfig.h"
#include "wServer.h"
#include "wChannelSocket.h"

namespace hnet {

wWorker::wWorker(std::string title, uint32_t slot, wMaster* master) : mMaster(master), mTitle(title), mPid(-1), mPriority(0), mRlimitCore(kRlimitCore), 
mDetached(0), mExited(0), mExiting(0), mStat(0), mRespawn(1), mJustSpawn(0), mTimeline(soft::TimeUnix()), mSlot(slot), mChannel(NULL) { }

wWorker::~wWorker() { }

int wWorker::PrepareStart() {
	// 设置当前进程优先级。进程默认优先级为0
	// -20 -> 20 高 -> 低。只有root可提高优先级，即可减少priority值
	if (mSlot < kMaxProcess && mPriority != 0) {
        if (setpriority(PRIO_PROCESS, 0, mPriority) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::PrepareStart setpriority() failed", error::Strerror(errno).c_str());
            return -1;
        }
    }
	
	// 设置进程的最大文件描述符
    if (mRlimitCore > 0) {
		struct rlimit rlmt;
        rlmt.rlim_cur = static_cast<rlim_t>(mRlimitCore);
        rlmt.rlim_max = static_cast<rlim_t>(mRlimitCore);
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::PrepareStart setrlimit() failed", error::Strerror(errno).c_str());
            return -1;
        }
    }
	
    // 每个worker进程，只保留自身worker进程的channel[1]与其他所有worker进程的channel[0]描述符
    for (uint32_t n = 0; n < kMaxProcess; n++) {
    	// 将其他进程的channel[1]关闭，自己的除外
    	if (n == mSlot || mMaster->Worker(n) == NULL || mMaster->Worker(n)->mPid == -1) {
    		continue;
    	} else if (mMaster->Worker(n)->ChannelClose(1) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::PrepareStart close(1) failed", error::Strerror(errno).c_str());
            return -1;
    	}
    }
    // 关闭该进程worker进程的channel[0]描述符
    if (mMaster->Worker(mSlot)->ChannelClose(0) == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::PrepareStart close(0) failed", error::Strerror(errno).c_str());
        return -1;
    }
    mTimeline = soft::TimeUnix();

	if (PrepareRun() == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::PrepareStart PrepareRun() failed", "");
		return -1;
	}
	
    // 子进程标题
    if (mMaster->Server()->Config()->Setproctitle(kWorkerTitle, mTitle.c_str(), false) == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::PrepareStart Setproctitle() failed", "");
        return -1;
    }
	return 0;
}

int wWorker::Start() {
	// worker进程中重启所有信号处理器
	wSigSet ss;
    ss.EmptySet();
	if (ss.Procmask(SIG_SETMASK) == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::Start Procmask() failed", "");
        return -1;
	}

    if (Run() == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::Start Run() failed", "");
        return -1;
    }
    
    // 启动server服务
    if (mMaster->Server()->WorkerStart() == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wWorker::Start WorkerStart() failed", "");
        return -1;
    }
	return 0;
}

}	// namespace hnet
