
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSignal.h"
#include "wMisc.h"
#include "wLogger.h"

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
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::wSignal sigemptyset() failed", error::Strerror(errno).c_str());
    }
    return -1;
}

int wSignal::AddMaskSet(int signo) {
    int ret = sigaddset(&mSigAct.sa_mask, signo);
    if (ret == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::AddMaskSet sigaddset() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSignal::AddSigno(int signo, struct sigaction *oact) {
    int ret = sigaction(signo, &mSigAct, oact);
    if (ret == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::AddSigno sigaction() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wSignal::AddHandler(const Signal_t *signal) {
    mSigAct.sa_handler = signal->mHandler;
    mSigAct.sa_flags = 0;
    int ret = sigemptyset(&mSigAct.sa_mask);
    if (ret == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSignal::AddHandler sigemptyset() failed", error::Strerror(errno).c_str());
        return ret;
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
        
    case SIGABRT:
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
