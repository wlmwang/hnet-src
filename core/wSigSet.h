
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SIG_SET_H_
#define _W_SIG_SET_H_

#include <signal.h>

#include "wCore.h"
#include "wNoncopyable.h"

class wSigSet : private wNoncopyable
{
	public:
		wSigSet()
		{
			EmptySet();
		}

		int EmptySet()
		{
			return sigemptyset(&mSet);
		}

		int FillSet()
		{
			return sigfillset(&mSet);
		}

		int AddSet(int signo)
		{
			return sigaddset(&mSet, signo);
		}
		
		int DelSet(int signo)
		{
			return sigdelset(&mSet, signo);
		}

		//若真则返回1,若假则返回0,若出错则返回-1
		int Ismember(int signo)
		{
			return sigismember(&mSet, signo);
		}

		/**
		 * 阻塞信号集
		 * @param  type SIG_BLOCK（与 mOldset 的并集为新信号集） 
		 *              SIG_UNBLOCK（解除阻塞的 mSet 信号集） 
		 *              SIG_SETMASK（新的信号屏蔽字设置为 mSet 所指向的信号集）
		 * @return      若成功则返回0,若出错则返回-1
		 */
		int Procmask(int iType = SIG_BLOCK, sigset_t *pOldSet = NULL)
		{
			return sigprocmask(iType, &mSet, pOldSet);
		}

		//进程未决的信号集。成功则返回0,若出错则返回-1
		int Pending(sigset_t *pPendSet)
		{
			return sigpending(pPendSet);
		}

		//阻塞等待信号集事件发生
		int Suspend()
		{
			return sigsuspend(&mSet);
		}

		virtual ~wSigSet() {}

	private:
		sigset_t mSet;	//设置信号集
};

#endif