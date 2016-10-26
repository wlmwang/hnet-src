
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SHMTX_H_
#define _W_SHMTX_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wShm;
class wSem;

// 多进程互斥量（主要用于父子进程）
// 基于信号量实现
class wShmtx : private wNoncopyable {
public:
	// 在共享建立sem  pShm共享内存  iSpin自旋初始值
	wStatus Create(wShm *pShm, int iSpin = 2048);

	wStatus Lock();

	wStatus Unlock();

	wStatus TryLock();
	
	// 自旋争抢锁
	void LockSpin();

private:
	wSem *mSem {NULL};
	int  mSpin {0};	
};

}	// namespace hnet


#endif
