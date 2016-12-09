
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */
 
#include "wShm.h"
#include "wMisc.h"

namespace hnet {

wPosixShm::wPosixShm(const std::string *filename, size_t size) : mShmId(-1), mFilename(*filename), mShmhead(NULL) {
	mSize = misc::Align(size + sizeof(struct Shmhead_t), kPageSize);
}

wPosixShm::~wPosixShm() {
	Destroy();
}

const wStatus& wPosixShm::CreateShm(char* ptr, int pipeid) {
	int fd = open(mFilename.c_str(), O_CREAT);
	if (fd == -1) {
		return mStatus = wStatus::IOError("wPosixShm::CreateShm, open() failed", strerror(errno));
	}
	close(fd);

	key_t key = ftok(mFilename.c_str(), pipeid);
	if (key == -1) {
		return mStatus = wStatus::IOError("wPosixShm::CreateShm, ftok() failed", strerror(errno));
	}

	mShmId = shmget(key, mSize, IPC_CREAT| IPC_EXCL| 0666);
	if (mShmId == -1) {
		// 申请内存失败
		if (errno != EEXIST) {
			return mStatus = wStatus::IOError("wPosixShm::CreateShm, shmget() failed", strerror(errno));
		}

		// 该内存已经被申请，申请访问控制它
		mShmId = shmget(key, mSize, 0666);
		if (mShmId == -1) {

			// 猜测是否是该内存大小太小，先获取内存ID
			mShmId = shmget(key, 0, 0666);
			if (mShmId == -1) {
				// 如果失败，则无法操作该内存，只能退出
				return mStatus = wStatus::IOError("wPosixShm::CreateShm, shmget() failed", strerror(errno));
			} else {
				// 如果成功，则先删除原内存
				if (shmctl(mShmId, IPC_RMID, NULL) == -1) {
					return mStatus = wStatus::IOError("wPosixShm::CreateShm, shmctl(IPC_RMID) failed", strerror(errno));
				}

				// 再次申请该ID的内存
				mShmId = shmget(key, mSize, IPC_CREAT| IPC_EXCL| 0666);
				if (mShmId == -1) {
					return mStatus = wStatus::IOError("wPosixShm::CreateShm, shmget() failed again", strerror(errno));
				}
			}
		}
	}
	
	// 映射地址
	char* addr = reinterpret_cast<char *>(shmat(mShmId, NULL, 0));
	if (addr == reinterpret_cast<char*>(-1)) {
    	return mStatus = wStatus::IOError("wPosixShm::CreateShm, shmat() failed", strerror(errno));
    }

	mShmhead = reinterpret_cast<struct Shmhead_t*>(addr);
	mShmhead->mStart = addr;
	mShmhead->mEnd = addr + mSize;
	mShmhead->mUsedOff = ptr = addr + sizeof(struct Shmhead_t);
	return mStatus.Clear();
}

const wStatus& wPosixShm::AttachShm(char* ptr, int pipeid) {
	key_t key = ftok(mFilename.c_str(), pipeid);
	if (key == -1) {
		return mStatus = wStatus::IOError("wPosixShm::AttachShm, ftok() failed", strerror(errno));
	}

	mShmId = shmget(key, mSize, 0666);
	if (mShmId == -1) {
		return mStatus = wStatus::IOError("wPosixShm::AttachShm, shmget() failed", strerror(errno));
	}
	
    // 映射地址
	char* addr = reinterpret_cast<char *>(shmat(mShmId, NULL, 0));
    if (addr == reinterpret_cast<char*>(-1)) {
    	return mStatus = wStatus::IOError("wPosixShm::AttachShm, shmat() failed", strerror(errno));
    }

	mShmhead = reinterpret_cast<struct Shmhead_t*>(addr);
	ptr = mShmhead->mUsedOff;
	return mStatus.Clear();
}

const wStatus& wPosixShm::AllocShm(char* ptr, size_t size) {
	if (mShmhead->mUsedOff + size < mShmhead->mEnd) {
		ptr = mShmhead->mUsedOff;
		mShmhead->mUsedOff += size;
		return mStatus.Clear();
	}
	return mStatus = wStatus::Corruption("wPosixShm::AllocShm failed", "shm space not enough");
}

const wStatus& wPosixShm::Destroy() {
	// 只有最后一个进程 shmctl(IPC_RMID) 有效
    if (shmdt(mShmhead->mStart) == -1) {
    	return mStatus = wStatus::IOError("wPosixShm::Destroy, shmdt() failed", strerror(errno));
    } else if (shmctl(mShmId, IPC_RMID, NULL) == -1) {
    	return mStatus = wStatus::IOError("wPosixShm::Destroy, shmctl(IPC_RMID) failed", strerror(errno));
    }
    return mStatus.Clear();
}

}	// namespace hnet
