
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSignal.h"

int g_terminate;
int g_quit;
int g_sigalrm;
int g_sigio;
int g_reap;
int g_reconfigure;

//信号集
wSignal::signal_t g_signals[] = {
	{SIGHUP, "SIGHUP", "reload", SignalHandler},
	{SIGTERM, "SIGTERM", "stop", SignalHandler},
	{SIGINT, "SIGINT", "", SignalHandler},
	{SIGQUIT, "SIGQUIT", "quit", SignalHandler},
	{SIGALRM, "SIGALRM", "", SignalHandler},
	{SIGIO, "SIGIO", "", SignalHandler},
	{SIGCHLD, "SIGCHLD", "", SignalHandler},
	{SIGSYS, "SIGSYS", "", SIG_IGN},
	{SIGPIPE, "SIGPIPE", "", SIG_IGN},
	{0, NULL, "", NULL}
};

wSignal::wSignal(__sighandler_t  func)
{
	mSigAct.sa_handler = func;
	mSigAct.sa_flags = 0;
	sigemptyset(&mSigAct.sa_mask);
}

int wSignal::AddMaskSet(int signo)
{
	return sigaddset(&mSigAct.sa_mask, signo);
}

int wSignal::AddHandler(__sighandler_t  func)
{
	mSigAct.sa_handler = func;
	mSigAct.sa_flags = 0;
	return sigemptyset(&mSigAct.sa_mask);
}

//添加信号处理
int wSignal::AddSigno(int signo, struct sigaction *oact)
{
	return sigaction(signo, &mSigAct, oact);
}

int wSignal::AddSig_t(const signal_t *pSig)
{
	int code = AddHandler(pSig->mHandler);
	if (code < 0)
	{
		return code;
	}
	return AddSigno(pSig->mSigno);
}

/**
 *  信号处理入口函数
 *  对于SIGCHLD信号，由自定义函数处理g_reap时调用waitpid
 */
void SignalHandler(int signo)
{
    string action;
    int err;
    wSignal::signal_t *sig;
    err = errno;
	
    for (sig = g_signals; sig->mSigno != 0; sig++) 
	{
        if (sig->mSigno == signo) 
		{
            break;
        }
    }

    action = "";
	switch (signo) 
	{
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
	
	LOG_DEBUG(ELOG_KEY, "[system] signal %d (%s) received%s", signo, sig->mSigname, action.c_str());
    errno = err;
}

