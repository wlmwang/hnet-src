
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSignal.h"

namespace hnet {

int g_terminate;
int g_quit;
int g_sigalrm;
int g_sigio;
int g_reap;
int g_reconfigure;

// 信号集
wSignal::Signal_t g_signals[] = {
    {SIGHUP, "SIGHUP", "reload", &wSignal::SignalHandler},
    {SIGTERM, "SIGTERM", "stop", &wSignal::SignalHandler},
    {SIGINT, "SIGINT", "", &wSignal::SignalHandler},
    {SIGQUIT, "SIGQUIT", "quit", &wSignal::SignalHandler},
    {SIGALRM, "SIGALRM", "", &wSignal::SignalHandler},
    {SIGIO, "SIGIO", "", &wSignal::SignalHandler},
    {SIGCHLD, "SIGCHLD", "", &wSignal::SignalHandler},
    {SIGSYS, "SIGSYS", "", SIG_IGN},
    {SIGPIPE, "SIGPIPE", "", SIG_IGN},
    {0, NULL, "", NULL}
};

wSignal::wSignal() : mStatus() { }

wSignal::wSignal(__sighandler_t  func) : mStatus() {
    mSigAct.sa_handler = func;
    mSigAct.sa_flags = 0;
    if (sigemptyset(&mSigAct.sa_mask) == -1) {
        mStatus = wStatus::IOError("wSignal::wSignal, sigemptyset failed", strerror(errno));
    }
}

wStatus wSignal::AddMaskSet(int signo) {
    if (sigaddset(&mSigAct.sa_mask, signo) == -1) {
        return mStatus = wStatus::IOError("wSignal::wSignal, sigaddset failed", strerror(errno));
    }
    return mStatus = wStatus::Nothing();
}

wStatus wSignal::AddSigno(int signo, struct sigaction *oact) {
    if (sigaction(signo, &mSigAct, oact) == -1) {
        mStatus = wStatus::IOError("wSignal::wSignal, sigaction failed", strerror(errno));
    }
    return mStatus = wStatus::Nothing();
}

wStatus wSignal::AddHandler(const Signal_t *signal) {
    mSigAct.sa_handler = signal->mHandler;
    mSigAct.sa_flags = 0;
    if (sigemptyset(&mSigAct.sa_mask) == -1) {
        return mStatus = wStatus::IOError("wSignal::wSignal, sigemptyset failed", strerror(errno));
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

    string action = "";
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
    
    // LOG_DEBUG(ELOG_KEY, "[system] signal %d (%s) received%s", signo, signal->mSigname, action.c_str());
    errno = err;
}

}   // namespace hnet