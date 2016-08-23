
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SIGNAL_H_
#define _W_SIGNAL_H_

#include <signal.h>		//typedef void (*sighandler_t)(int);

#include "wCore.h"
#include "wLog.h"
#include "wNoncopyable.h"

void SignalHandler(int signo);

class wSignal : private wNoncopyable
{
	public:
		struct signal_t
		{
		    int     mSigno;
		    char   *mSigname;	//信号的字符串表现形式，如"SIGIO"
		    char   *mName;		//信号的名称，如"stop"
		    __sighandler_t mHandler;

		    signal_t(int signo, const char *signame, const char *name, __sighandler_t func)
		    {
		    	mSigno = signo;
		    	mSigname = (char *) signame;
		    	mName = (char *) name;
		    	mHandler = func;
		    }
		};

		wSignal() {}
		wSignal(__sighandler_t  func);	//SIG_DFL(采用缺省的处理方式)，也可以为SIG_IGN

		int AddMaskSet(int signo);	//添加屏蔽集
		int AddHandler(__sighandler_t  func);
		//添加信号处理
		int AddSigno(int signo, struct sigaction *oact = NULL);
		int AddSig_t(const signal_t *pSig);

	protected:
		struct sigaction mSigAct;
};

//信号集
extern wSignal::signal_t g_signals[];

extern int g_quit;		//SIGQUIT
extern int g_terminate;	//SIGTERM SIGINT
extern int g_reconfigure;//SIGHUP
extern int g_sigalrm;	//SIGALRM
extern int g_sigio;		//SIGIO
extern int g_reap;		//SIGCHLD
//extern int g_restart;
//extern int g_reopen;

#endif