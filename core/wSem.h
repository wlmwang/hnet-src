
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SEM_H_
#define _W_SEM_H_

#include <semaphore.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

// 信号量接口类
// 用于多进程同步操作
class wSem : private wNoncopyable {
public:
	virtual ~wSem() { }

	// 创建无名信号量
	virtual const wStatus& Open(int oflag = O_CREAT, mode_t mode= 0644, unsigned int value = 1) = 0;

	// 阻塞等待信号，获取拥有权（P操作）
	virtual const wStatus& Wait() = 0;

	// 非阻塞版本 等待信号，获取拥有权（P操作）
	virtual const wStatus& TryWait() = 0;

	// 发出信号即释放拥有权（V操作）
	virtual const wStatus& Post() = 0;

	// 删除系统中的信号量（一定要清除，否则即使进程退出，系统也会占用该资源。/dev/shm中一个文件）
	virtual const wStatus& Destroy() = 0;
};


// 信号量实现类(有名 || 匿名)
class wPosixSem : public wSem {
public:
	wPosixSem(const std::string& name) : mName(name) { }

	virtual ~wPosixSem() {
		Destroy();
    }

	virtual const wStatus& Open(int oflag = O_CREAT, mode_t mode = 0644, unsigned int value = 1/** 初值为value*/) {
		if (mName.size() > 0) {	// 有名信号量
			sem_t* id = sem_open(mName.c_str(), oflag, mode, value);
			if (id == SEM_FAILED) {
				mStatus = wStatus::IOError("wPosixSem::Open, sem_open() failed", error::Strerror(errno));
			}
			mSem = *id;
		} else {	// 匿名信号量
			if (sem_init(&mSem, 0, value) == -1) {
				mStatus = wStatus::IOError("wPosixSem::Open, sem_init() failed", error::Strerror(errno));
			}
		}
		return mStatus;
	}

	virtual const wStatus& Wait() {
        if (sem_wait(&mSem) == -1) {
            return mStatus = wStatus::IOError("wPosixSem::Wait, sem_wait() failed", error::Strerror(errno));
        }
        return mStatus.Clear();
    }

	virtual const wStatus& TryWait() {
        if (sem_trywait(&mSem) == -1) {	// 未能获取锁
            return mStatus = wStatus::IOError("wPosixSem::TryWait, sem_trywait() failed", error::Strerror(errno), false);
        }
        return mStatus.Clear();
    }

	virtual const wStatus& Post() {
        if (sem_post(&mSem) == -1) {
            return mStatus = wStatus::IOError("wPosixSem::Post, sem_post() failed", error::Strerror(errno));
        }
        return mStatus.Clear();
    }

	virtual const wStatus& Destroy() {
		if (mName.size() > 0) {
	        if (sem_close(&mSem) == -1) {	// 关闭当前进程中的互斥量句柄
	            mStatus = wStatus::IOError("wPosixSem::Destroy, sem_close() failed", error::Strerror(errno));
	        }
	        if (sem_unlink(mName.c_str()) == -1) {	// 只有最后一个进程 sem_unlink 有效
	        	mStatus = wStatus::IOError("wPosixSem::Destroy, sem_unlink() failed", error::Strerror(errno));
	        }
		} else {
	        if (sem_destroy(&mSem) == -1) {
	            return mStatus = wStatus::IOError("wPosixSem::Destroy, sem_destroy() failed", error::Strerror(errno));
	        }
		}
		return mStatus;
	}

protected:
	sem_t mSem;
	std::string mName;
	wStatus mStatus;
};

}	// namespace hnet

#endif
