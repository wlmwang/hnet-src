
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */
 
#include "wShm.h"
#include "wMisc.h"
#include "wLogger.h"

namespace hnet {

wPosixShm::wPosixShm(const std::string& filename, size_t size) : mShmId(-1), mShmhead(NULL), mFilename(filename) {
	mSize = misc::Align(size + sizeof(struct Shmhead_t), kPageSize);
}

wPosixShm::~wPosixShm() {
	Destroy();
}

int wPosixShm::CreateShm(int pipeid) {
	int fd = open(mFilename.c_str(), O_CREAT);
	if (fd == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm open() failed", error::Strerror(errno).c_str());
		return -1;
	}
	close(fd);

	key_t key = ftok(mFilename.c_str(), pipeid);
	if (key == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm ftok() failed", error::Strerror(errno).c_str());
		return -1;
	}

	mShmId = shmget(key, mSize, IPC_CREAT| IPC_EXCL| 0666);
	if (mShmId == -1) {
		// 申请内存失败
		if (errno != EEXIST) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm shmget() failed", error::Strerror(errno).c_str());
			return -1;
		}

		// 该内存已经被申请，申请访问控制它
		mShmId = shmget(key, mSize, 0666);
		if (mShmId == -1) {

			// 猜测是否是该内存大小太小，先获取内存ID
			mShmId = shmget(key, 0, 0666);
			if (mShmId == -1) {
				// 如果失败，则无法操作该内存，只能退出
				LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm shmget() failed", error::Strerror(errno).c_str());
				return -1;
			} else {
				// 如果成功，则先删除原内存
				if (shmctl(mShmId, IPC_RMID, NULL) == -1) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm shmctl() failed", error::Strerror(errno).c_str());
					return -1;
				}

				// 再次申请该ID的内存
				mShmId = shmget(key, mSize, IPC_CREAT| IPC_EXCL| 0666);
				if (mShmId == -1) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm shmget() failed", error::Strerror(errno).c_str());
					return -1;
				}
			}
		}
	}
	
	// 映射地址
	void* addr = shmat(mShmId, NULL, 0);
	if ((intptr_t)addr == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::CreateShm shmat() failed", error::Strerror(errno).c_str());
		return -1;
    }

    // 存储头信息
	mShmhead = reinterpret_cast<struct Shmhead_t*>(addr);
	mShmhead->mStart = reinterpret_cast<uintptr_t>(addr);
	mShmhead->mUsedOff = mShmhead->mStart + static_cast<uintptr_t>(sizeof(struct Shmhead_t));
	mShmhead->mEnd = mShmhead->mStart + static_cast<uintptr_t>(mSize);
	return 0;
}

int wPosixShm::AttachShm(int pipeid) {
	key_t key = ftok(mFilename.c_str(), pipeid);
	if (key == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::AttachShm ftok() failed", error::Strerror(errno).c_str());
		return -1;
	}

	mShmId = shmget(key, mSize, 0666);
	if (mShmId == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::AttachShm shmget() failed", error::Strerror(errno).c_str());
		return -1;
	}
	
    // 映射地址
	void* addr = shmat(mShmId, NULL, 0);
	if ((intptr_t)addr == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::AttachShm shmat() failed", error::Strerror(errno).c_str());
		return -1;
    }

    // 存储头信息
	mShmhead = reinterpret_cast<struct Shmhead_t*>(addr);
	return 0;
}

void* wPosixShm::AllocShm(size_t size) {
	if (mShmhead->mUsedOff + static_cast<uintptr_t>(size) < mShmhead->mEnd) {
		void* ptr = (void*)(mShmhead->mUsedOff);
		mShmhead->mUsedOff += static_cast<uintptr_t>(size);
		return ptr;
	}
	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::AllocShm failed, shm space not enough", "");
	return NULL;
}

void wPosixShm::Destroy() {
    if (mShmhead && mShmhead->mStart && shmdt(reinterpret_cast<void*>(mShmhead->mStart)) == -1) {	// 关闭当前进程中的共享内存句柄
    	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::Destroy shmdt() failed", error::Strerror(errno).c_str());
    } 
    if (mShmId > 0 && shmctl(mShmId, IPC_RMID, NULL) == -1) {	// 只有最后一个进程 shmctl() IPC_RMID 有效
    	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixShm::Destroy shmctl() failed", error::Strerror(errno).c_str());
    }
}

}	// namespace hnet
