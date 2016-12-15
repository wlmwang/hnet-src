
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_THREAD_H_
#define _W_THREAD_H_

#include <pthread.h>
#include <cstdarg>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wMutex;
class wCond;

class wThread : private wNoncopyable {
public:
	// join 标明这个线程退出的时候是否保存状态，如果为true表示线程退出保存状态，否则将不保存退出状态
    wThread(bool join = true);
    virtual ~wThread();

    const wStatus& StartThread();
    const wStatus& JoinThread();

    virtual const wStatus& RunThread() = 0;

    virtual const wStatus& PrepareThread() {
    	return mStatus;
    }

    inline bool Joinable() {
    	return mJoinable;
    }

protected:
    static void* ThreadWrapper(void *argv);

    pthread_attr_t mAttr;
    pthread_t mPthreadId;
    bool mJoinable;
    bool mAlive;
    wMutex *mMutex;
    wCond *mCond;

    wStatus mStatus;
};

}	// namespace hnet

#endif
