
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SIGNAL_H_
#define _W_SIGNAL_H_

#include <signal.h>		//typedef void (*sighandler_t)(int);
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wSignal : private wNoncopyable {
public:
    struct Signal_t;

    wSignal();

    // SIG_DFL(采用缺省的处理方式)，也可以为SIG_IGN
    wSignal(__sighandler_t  func);
    
    // 添加信号处理
    const wStatus& AddSigno(int signo, struct sigaction *oact = NULL);

    const wStatus& AddHandler(const Signal_t *signal);

    // 添加屏蔽集
    const wStatus& AddMaskSet(int signo);

    static void SignalHandler(int signo);
    
protected:
    wStatus mStatus;
    struct sigaction mSigAct;
};

struct wSignal::Signal_t {
    Signal_t(int signo, const char *signame, const char *name, __sighandler_t func) {
        mSigno = signo;
        mSigname = (char *) signame;
        mName = (char *) name;
        mHandler = func;
    }

    int     mSigno;
    char   *mSigname;   // 信号的字符串表现形式，如"SIGIO"
    char   *mName;      // 信号的名称，如"stop"
    __sighandler_t mHandler;
};

// 信号集
extern wSignal::Signal_t g_signals[];
extern int g_sigalrm;       // SIGALRM
extern int g_sigio;         // SIGIO
extern int g_terminate;     // SIGTERM SIGINT
extern int g_quit;          // SIGQUIT
extern int g_reconfigure;   // SIGHUP
extern int g_reap;          // SIGCHLD
extern int g_reopen;        // SIGUSR1

}   // namespace hnet

#endif
