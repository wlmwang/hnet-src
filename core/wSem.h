
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SEM_H_
#define _W_SEM_H_

#include <sys/ipc.h>
#include <sys/sem.h>
#include <semaphore.h>

#include "wCore.h"
#include "wLog.h"
#include "wNoncopyable.h"

// 信号量操作。主要应用在多进程间互斥锁
namespace hnet {
class wSem : private wNoncopyable {
public:
	// pshared 0线程共享 1进程共享
	// value 信号量初值，最大值为SEM_VALUE_MAX
	wSem(int pshared = 0, int value = 0) : mPshared(pshared), mValue(value) {
        int iRet = sem_init(&mSem, mPshared, mValue);
        assert(iRet >= 0);
	}
	
	~wSem() {
        sem_destroy(&mSem);
	}

	// 阻塞等待信号，获取拥有权（原子的从信号量的值减去一个"1"）
	// ==0成功 	==-1失败（设置errno）
	// EINTR 调用被信号处理中断
	// EINVAL 不是有效的信号量
	int Wait() {
        return sem_wait(&mSem);
	}
	
	// 等待信号，获取拥有权（可以获取时，直接将信号量sem减1，否则返回错误代码）
	// ==0成功 	==-1失败（设置errno）
	// EAGAIN 除了锁定无法进行别的操作(如信号量当前是0值)
	int TryWait() {
        return sem_trywait(&mSem);
	}

	// 发出信号即释放拥有权（原子的从信号量的值增加一个"1"）
	// ==0成功 ==-1失败（设置errno）
	int Post() {
        return sem_post(&mSem);
	}

protected:
    int mErr;
    sem_t mSem;
    int mPshared;
    int mValue;
};

}	// namespace hnet

#endif
