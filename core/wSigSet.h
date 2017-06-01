
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SIG_SET_H_
#define _W_SIG_SET_H_

#include <signal.h>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

class wSigSet : private wNoncopyable {
public:
    wSigSet();
    int EmptySet();
    int FillSet();
    int AddSet(int signo);
    int DelSet(int signo);

    // 若真则返回1,若假则返回0,若出错则返回-1
    int Ismember(int signo);

    // 阻塞信号集
    // SIG_BLOCK（与 mOldset 的并集为新信号集）
    // SIG_UNBLOCK（解除阻塞的 mSet 信号集）
    // SIG_SETMASK（新的信号屏蔽字设置为 mSet 所指向的信号集）
    int Procmask(int type = SIG_BLOCK, sigset_t *oldset = NULL);

    // 进程未决的信号集
    int Pending(sigset_t *pPendSet);

    // 阻塞等待信号集事件发生
    int Suspend();

private:
    sigset_t mSet;	// 设置信号集
};

}	// namespace hnet

#endif
