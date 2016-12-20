
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSignal.h"

namespace hnet {

int g_sigalrm = 0;
int g_sigio = 0;
int g_terminate = 0;
int g_quit = 0;
int g_reconfigure = 0;
int g_reap = 0;
int g_reopen = 0;

// 信号集
wSignal::Signal_t g_signals[] = {
    {SIGHUP,    "SIGHUP",   "restart",  &wSignal::SignalHandler},   // 重启
    {SIGUSR1,   "SIGUSR1",  "reopen",   &wSignal::SignalHandler},   // 清除日志
    {SIGQUIT,   "SIGQUIT",  "quit",     &wSignal::SignalHandler},   // 优雅退出
    {SIGTERM,   "SIGTERM",  "stop",     &wSignal::SignalHandler},   // 立即退出
    {SIGINT,    "SIGINT",   "",         &wSignal::SignalHandler},   // 立即退出
    {SIGALRM,   "SIGALRM",  "",         &wSignal::SignalHandler},
    {SIGIO,     "SIGIO",    "",         &wSignal::SignalHandler},
    {SIGCHLD,   "SIGCHLD",  "",         &wSignal::SignalHandler},
    {SIGSYS,    "SIGSYS",   "",         SIG_IGN},
    {SIGPIPE,   "SIGPIPE",  "",         SIG_IGN},
    {0,         NULL,       "",         NULL}
};

wSignal::wSignal() { }

wSignal::wSignal(__sighandler_t  func) {
    mSigAct.sa_handler = func;
    mSigAct.sa_flags = 0;
    if (sigemptyset(&mSigAct.sa_mask) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        mStatus = wStatus::IOError("wSignal::wSignal, sigemptyset failed", err);
    }
}

const wStatus& wSignal::AddMaskSet(int signo) {
    if (sigaddset(&mSigAct.sa_mask, signo) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        return mStatus = wStatus::IOError("wSignal::wSignal, sigaddset failed", err);
    }
    return mStatus.Clear();
}

const wStatus& wSignal::AddSigno(int signo, struct sigaction *oact) {
    if (sigaction(signo, &mSigAct, oact) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        mStatus = wStatus::IOError("wSignal::wSignal, sigaction failed", err);
    }
    return mStatus.Clear();
}

const wStatus& wSignal::AddHandler(const Signal_t *signal) {
    mSigAct.sa_handler = signal->mHandler;
    mSigAct.sa_flags = 0;
    if (sigemptyset(&mSigAct.sa_mask) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        return mStatus = wStatus::IOError("wSignal::wSignal, sigemptyset failed", err);
    }
    return AddSigno(signal->mSigno);
}

// 信号处理入口函数
// 对于SIGCHLD信号，由自定义函数处理g_reap时调用waitpid
void wSignal::SignalHandler(int signo) {
    int err = errno;
    wSignal::Signal_t *signal;
    for (signal = g_signals; signal->mSigno != 0; signal++) {
        if (signal->mSigno == signo) {
            break;
        }
    }

    std::string action = "";
    switch (signo) {
    case SIGQUIT:
        g_quit = 1;
        action = ", shutting down";
        break;

    case SIGTERM:
    case SIGINT:
        g_terminate = 1;
        action = ", exiting";
        break;

    case SIGHUP:
        g_reconfigure = 1;
        action = ", reconfiguring";
        break;
    
    case SIGUSR1:
        g_reopen = 1;
        action = ", reopen";
        break;

    case SIGALRM:
        g_sigalrm = 1;
        break;

    case SIGIO:
        g_sigio = 1;
        break;

    case SIGCHLD:
        g_reap = 1;
        break;
    }
    
    errno = err;
}

}   // namespace hnet
