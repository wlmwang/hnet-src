
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */
 
#include "wSem.h"
#include "wMisc.h"
#include "wLogger.h"

namespace hnet {

wPosixSem::wPosixSem(const std::string& name) : mName(name) {
    memset(&mSem, 0, sizeof(mSem));
}

wPosixSem::~wPosixSem() {
    Remove();
}

int wPosixSem::Open(int oflag, mode_t mode, unsigned int value) {
	if (mName.size() > 0) {	// 有名信号量
		sem_t* id = sem_open(mName.c_str(), oflag, mode, value);
		if (id == SEM_FAILED) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Open sem_open() failed", error::Strerror(errno).c_str());
			return -1;
		}
		mSem = *id;
	} else {	// 匿名信号量
		if (sem_init(&mSem, 0, value) == -1) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Open sem_init() failed", error::Strerror(errno).c_str());
			return -1;
		}
	}
	return 0;
}

int wPosixSem::Wait() {
    int ret = sem_wait(&mSem);
    if (ret == -1) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Wait sem_wait() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wPosixSem::TryWait() {
    int ret = sem_trywait(&mSem);
    if (ret == -1) {
        //H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::TryWait sem_trywait() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

int wPosixSem::Post() {
    int ret = sem_post(&mSem);
    if (ret == -1) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Post sem_post() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

void wPosixSem::Destroy() {
	if (mName.size() > 0) {
        // 只有最后一个进程 sem_unlink 有效
        if (sem_unlink(mName.c_str()) == -1) {
        	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Destroy sem_unlink() failed", error::Strerror(errno).c_str());
        }
	} else {
        if (sem_destroy(&mSem) == -1) {
            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Destroy sem_destroy() failed", error::Strerror(errno).c_str());
        }
	}
}

void wPosixSem::Remove() {
    if (mName.size() > 0) {
        // 关闭当前进程中的互斥量句柄
        if (sem_close(&mSem) == -1) {
            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSem::Destroy sem_close() failed", error::Strerror(errno).c_str());
        }
    }
}

}	// namespace hnet