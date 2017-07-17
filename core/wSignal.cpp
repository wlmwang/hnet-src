
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSignal.h"
#include "wMisc.h"
#include "wLogger.h"

namespace hnet {

volatile int hnet_sigalrm = 0;
volatile int hnet_sigio = 0;
volatile int hnet_terminate = 0;
volatile int hnet_quit = 0;
volatile int hnet_reconfigure = 0;
volatile int hnet_reap = 0;
volatile int hnet_reopen = 0;

// 信号集
wSignal::Signal_t hnet_signals[] = {
    {SIGHUP,    "SIGHUP",   "restart",  &wSignal::SignalHandler},   // 重启
    {SIGUSR1,   "SIGUSR1",  "reopen",   &wSignal::SignalHandler},   // 清除日志
    {SIGQUIT,   "SIGQUIT",  "quit",     &wSignal::SignalHandler},   // 优雅退出
    {SIGTERM,   "SIGTERM",  "stop",     &wSignal::SignalHandler},   // 立即退出
    {SIGINT,    "SIGINT",   "",         &wSignal::SignalHandler},   // 立即退出
    {SIGALRM,   "SIGALRM",  "",         &wSignal::SignalHandler},
    {SIGIO,     "SIGIO",    "",         &wSignal::SignalHandler},
    {SIGCHLD,   "SIGCHLD",  "",         &wSignal::SignalHandler},
    {SIGABRT,   "SIGABRT",  "",         &wSignal::SignalHandler},

    {SIGSYS,    "SIGSYS",   "",         SIG_IGN},
    {SIGPIPE,   "SIGPIPE",  "",         SIG_IGN},
    {SIGTSTP,   "SIGTSTP",  "",         SIG_IGN},   // Ctrl-Z
    {0,         NULL,       "",         NULL}
};

wSignal::wSignal() {
    memset(&mSigAct, 0, sizeof(mSigAct));
}

int wSignal::EmptySet(__sighandler_t  func) {
    mSigAct.sa_handler = func;
    mSigAct.sa_flags = 0;
    int ret = sigemptyset(&mSigAct.sa_mask);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::wSignal sigemptyset() failed", error::Strerror(errno).c_str());
    }
    return -1;
}

int wSignal::AddMaskSet(int signo) {
    int ret = sigaddset(&mSigAct.sa_mask, signo);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::AddMaskSet sigaddset() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSignal::AddSigno(int signo, struct sigaction *oact) {
    int ret = sigaction(signo, &mSigAct, oact);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::AddSigno sigaction() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSignal::AddHandler(const Signal_t *signal) {
    mSigAct.sa_handler = signal->mHandler;
    mSigAct.sa_flags = 0;
    int ret = sigemptyset(&mSigAct.sa_mask);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::AddHandler sigemptyset() failed", error::Strerror(errno).c_str());
        return ret;
    }
    return AddSigno(signal->mSigno);
}

// 信号处理入口函数
// 对于SIGCHLD信号，由自定义函数处理hnet_reap时调用waitpid
void wSignal::SignalHandler(int signo) {
    soft::TimeUpdate();
    
    int err = errno;
    wSignal::Signal_t *signal;
    for (signal = hnet_signals; signal->mSigno != 0; signal++) {
        if (signal->mSigno == signo) {
            break;
        }
    }

    std::string action = "";
    switch (signo) {
    case SIGQUIT:
        hnet_quit = 1;
        action = ", shutting down";
        break;
        
    case SIGABRT:
    case SIGTERM:
    case SIGINT:
        hnet_terminate = 1;
        action = ", exiting";
        break;

    case SIGHUP:
        hnet_reconfigure = 1;
        action = ", reconfiguring";
        break;
    
    case SIGUSR1:
        hnet_reopen = 1;
        action = ", reopen";
        break;

    case SIGALRM:
        hnet_sigalrm = 1;
        break;

    case SIGIO:
        hnet_sigio = 1;
        break;

    case SIGCHLD:
        hnet_reap = 1;
        break;
    }
    errno = err;
}

}   // namespace hnet
