
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SEM_H_
#define _W_SEM_H_

#include <semaphore.h>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

// 信号量接口类
// 用于多进程同步操作
class wSem : private wNoncopyable {
public:
	virtual ~wSem() { }

	// 创建无名信号量
	virtual int Open(int oflag = O_CREAT, mode_t mode= 0644, unsigned int value = 1/** 初值为value*/) = 0;

	// 阻塞等待信号，获取拥有权（P操作）
	virtual int Wait() = 0;

	// 非阻塞版本 等待信号，获取拥有权（P操作）
	virtual int TryWait() = 0;

	// 发出信号即释放拥有权（V操作）
	virtual int Post() = 0;

	// 删除系统中的信号量（一定要清除，否则即使进程退出，系统也会占用该资源。/dev/shm中一个文件）
	virtual void Destroy() = 0;
};


// 信号量实现类(有名 || 匿名)
class wPosixSem : public wSem {
public:
	wPosixSem(const std::string& name);
	virtual ~wPosixSem();

	virtual int Open(int oflag = O_CREAT, mode_t mode = 0644, unsigned int value = 1);
	virtual int Wait();
	virtual int TryWait();
	virtual int Post();
	virtual void Destroy();

protected:
	sem_t mSem;
	std::string mName;
};

}	// namespace hnet

#endif
