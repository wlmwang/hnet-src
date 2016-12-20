
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SIG_SET_H_
#define _W_SIG_SET_H_

#include <signal.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wSigSet : private wNoncopyable {
public:
    wSigSet() : mStatus() {
        if (sigemptyset(&mSet) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            mStatus = wStatus::IOError("wSigSet::wSigSet failed", err);
        }
    }

    const wStatus& FillSet() {
        if (sigfillset(&mSet) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            return mStatus = wStatus::IOError("wSigSet::FillSet failed", err);
        }
        return mStatus.Clear();
    }

    const wStatus& AddSet(int signo) {
        if (sigaddset(&mSet, signo) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            return mStatus = wStatus::IOError("wSigSet::AddSet failed", err);
        }
        return mStatus.Clear();
    }

    const wStatus& DelSet(int signo) {
        if (sigdelset(&mSet, signo) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            return mStatus = wStatus::IOError("wSigSet::AddSet failed", err);
        }
        return mStatus.Clear();
    }

    // 若真则返回1,若假则返回0,若出错则返回-1
    const wStatus& Ismember(int signo) {
        if (sigismember(&mSet, signo) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            return mStatus = wStatus::IOError("wSigSet::AddSet failed", err);
        }
        return mStatus.Clear();
    }

    // 阻塞信号集
    // SIG_BLOCK（与 mOldset 的并集为新信号集）
    // SIG_UNBLOCK（解除阻塞的 mSet 信号集）
    // SIG_SETMASK（新的信号屏蔽字设置为 mSet 所指向的信号集）
    const wStatus& Procmask(int iType = SIG_BLOCK, sigset_t *pOldSet = NULL) {
        if (sigprocmask(iType, &mSet, pOldSet) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            return mStatus = wStatus::IOError("wSigSet::Procmask failed", err);
        }
        return mStatus.Clear();
    }

    // 进程未决的信号集
    const wStatus& Pending(sigset_t *pPendSet) {
        if (sigpending(pPendSet) == -1) {
        	char err[kMaxErrorLen];
        	::strerror_r(errno, err, kMaxErrorLen);
            return mStatus = wStatus::IOError("wSigSet::Pending failed", err);
        }
        return mStatus.Clear();
    }

    // 阻塞等待信号集事件发生
    int Suspend() {
        return sigsuspend(&mSet);
    }

private:
    wStatus mStatus;
    sigset_t mSet;	//设置信号集
};

}	// namespace hnet

#endif
