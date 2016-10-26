
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SEM_H_
#define _W_SEM_H_

#include <sys/ipc.h>
#include <semaphore.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

// 信号量接口类
// 进程同步
class wSem : private wNoncopyable {
public:
	wSem() { }

	virtual ~wSem() { }

	virtual wStatus Open(const std::string& devshm, int oflag = O_CREAT, mode_t mode= 0644, unsigned int value = 1) = 0;

	// 阻塞等待信号，获取拥有权（P操作）
	virtual wStatus Wait() = 0;

	// 非阻塞版本 等待信号，获取拥有权（P操作）
	virtual wStatus TryWait() = 0;

	// 发出信号即释放拥有权（V操作）
	virtual wStatus Post() = 0;

	// 关闭信号量
	virtual wStatus Close() = 0;

	// 删除系统中的信号量（一定要清除，否则即使进程退出，系统也会占用该资源。/dev/shm中一个文件）
	virtual wStatus Destroy() = 0;
};


// 信号量实现类

// 有名信号量操作类
class wPosixSem : public wSem {
public:
	wPosixSem() { }

	virtual ~wPosixSem() {
		Close();
		Destroy();
    }

	virtual wStatus Open(const std::string& devshm, int oflag = O_CREAT, mode_t mode = 0644, unsigned int value = 1) {
		mDevshm = devshm;
		mSemId = sem_open(devshm.c_str(), oflag, mode, value);
		if (mSemId == SEM_FAILED) {
			return mStatus = wStatus::IOError("wPosixSem::Open", strerror(error));
		}
		return mStatus = wStatus::Nothing();
	}

	virtual wStatus Wait() {
        if (sem_wait(mSemId) == -1) {
            return mStatus = wStatus::IOError("wSem::Wait", strerror(error));
        }
        return mStatus = wStatus::Nothing();
    }

	virtual wStatus TryWait() {
        if (sem_trywait(mSemId) == -1) {
            return mStatus = wStatus::IOError("wSem::TryWait", strerror(error));	// EAGAIN
        }
        return mStatus = wStatus::Nothing();
    }

	virtual wStatus Post() {
        if (sem_post(mSemId) == -1) {
            return mStatus = wStatus::IOError("wSem::Post", strerror(error));
        }
        return mStatus = wStatus::Nothing();
    }

	virtual wStatus Close() {
        if (sem_close(mSemId) == -1) {
            mStatus = wStatus::IOError("wPosixSem::Close", strerror(error));
        }
	}

	virtual wStatus Destroy() {
        if (sem_unlink(mDevshm.c_str()) == -1) {
            mStatus = wStatus::IOError("wPosixSem::Close", strerror(error));
        }
	}
protected:
    wStatus mStatus;
    sem_t *mSemId;
    std::string mDevshm;
};

}	// namespace hnet

#endif
