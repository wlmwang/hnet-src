
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */
 
#include "wSigSet.h"
#include "wMisc.h"
#include "wLogger.h"

namespace hnet {

wSigSet::wSigSet() {
    memset(&mSet, 0, sizeof(mSet));
}

int wSigSet::EmptySet() {
    int ret = sigemptyset(&mSet);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::EmptySet sigemptyset() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSigSet::FillSet() {
	int ret = sigfillset(&mSet);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::FillSet sigfillset() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSigSet::AddSet(int signo) {
	int ret = sigaddset(&mSet, signo);
    if (ret == -1) {
    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::AddSet sigaddset() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSigSet::DelSet(int signo) {
	int ret = sigdelset(&mSet, signo);
    if (ret == -1) {
    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::DelSet sigdelset() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

// 若真则返回1,若假则返回0,若出错则返回-1
int wSigSet::Ismember(int signo) {
	int ret = sigismember(&mSet, signo);
    if (ret == -1) {
    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::Ismember sigismember() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

// 阻塞信号集
// SIG_BLOCK（与 mOldset 的并集为新信号集）
// SIG_UNBLOCK（解除阻塞的 mSet 信号集）
// SIG_SETMASK（新的信号屏蔽字设置为 mSet 所指向的信号集）
int wSigSet::Procmask(int type, sigset_t *oldset) {
	int ret = sigprocmask(type, &mSet, oldset);
    if (ret == -1) {
    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::Procmask sigprocmask() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

// 进程未决的信号集
int wSigSet::Pending(sigset_t *pPendSet) {
	int ret = sigpending(pPendSet);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::Pending sigpending() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

// 阻塞等待信号集事件发生
int wSigSet::Suspend() {
	int ret = sigsuspend(&mSet);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSigSet::Suspend sigsuspend() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

}	// namespace hnet
