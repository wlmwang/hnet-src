
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
	wSem(const std::string* devshm = NULL) { }

	virtual ~wSem() { }

	// devshm = NULL时有创建无名信号量，否则创建有名信号量
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


// 信号量实现类

// 信号量操作类(有名&&无名)
class wPosixSem : public wSem {
public:
	wPosixSem(const std::string* devshm = NULL) {
		if (devshm != NULL) {
			mDevshm = *devshm;
		}
	}

	virtual ~wPosixSem() {
		Destroy();
    }

	virtual const wStatus& Open(int oflag = O_CREAT, mode_t mode = 0644, unsigned int value = 1) {
		if (mDevshm.size() > 0) {
			// 有名信号量
			sem_t* id = sem_open(mDevshm.c_str(), oflag, mode, value);
			if (id == SEM_FAILED) {
				return mStatus = wStatus::IOError("wPosixSem::Open, sem_open() failed", strerror(errno));
			}
			mSemId = *id;
		} else {
			// 无名信号量
			if (sem_init(&mSemId, 1 /* 多进程共享 */, value /* 初值为value */) == -1) {
				return mStatus = wStatus::IOError("wPosixSem::Open, sem_init() failed", strerror(errno));
			}
		}
		return mStatus.Clear();
	}

	virtual const wStatus& Wait() {
        if (sem_wait(&mSemId) == -1) {
            return mStatus = wStatus::IOError("wPosixSem::Wait, sem_wait() failed", strerror(errno));
        }
        return mStatus.Clear();
    }

	virtual const wStatus& TryWait() {
        if (sem_trywait(&mSemId) == -1) {
            return mStatus = wStatus::IOError("wPosixSem::TryWait, sem_trywait() failed", strerror(errno));	// EAGAIN
        }
        return mStatus.Clear();
    }

	virtual const wStatus& Post() {
        if (sem_post(&mSemId) == -1) {
            return mStatus = wStatus::IOError("wPosixSem::Post, sem_post() failed", strerror(errno));
        }
        return mStatus.Clear();
    }

	virtual const wStatus& Destroy() {
		if (mDevshm.size() > 0) {
			// 只有最后一个进程 sem_unlink 有效
	        if (sem_close(&mSemId) == -1) {
	            return mStatus = wStatus::IOError("wPosixSem::Destroy, sem_close() failed", strerror(errno));
	        } else if (sem_unlink(mDevshm.c_str()) == -1) {
	        	return mStatus = wStatus::IOError("wPosixSem::Destroy, sem_unlink() failed", strerror(errno));
	        }
		} else {
	        if (sem_destroy(&mSemId) == -1) {
	            return mStatus = wStatus::IOError("wPosixSem::Destroy, sem_destroy() failed", strerror(errno));
	        }
		}
		return mStatus.Clear();
	}

protected:
	sem_t mSemId;
	std::string mDevshm;
	wStatus mStatus;
};

}	// namespace hnet

#endif
