
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SHM_H_
#define _W_SHM_H_

#include <sys/ipc.h>
#include <sys/shm.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

// 共享内存管理
class wShm : private wNoncopyable {
public:
	virtual ~wShm() { }

	virtual const wStatus& CreateShm(char* ptr, int pipeid = 'i') = 0;
	virtual const wStatus& AttachShm(char* ptr, int pipeid = 'i') = 0;
	virtual const wStatus& AllocShm(char* ptr, size_t size = 0) = 0;
	virtual const wStatus& Destroy() = 0;
};


// 共享内存实现类

// 共享内存额外存储头信息Shmhead_t结构体
struct Shmhead_t {
	// 共享内存开始地址
	char *mStart;

	// 共享内存结束地址
	char *mEnd;

	// 已被分配共享内存偏移地址offset，即下次可使用的开始地址
	char *mUsedOff;
};

class wPosixShm : public wShm {
public:
	wPosixShm(const std::string *filename, size_t size = kMsgQueueLen);
	virtual ~wPosixShm();

	const wStatus& CreateShm(char* ptr, int pipeid = 'i');
	const wStatus& AttachShm(char* ptr, int pipeid = 'i');
	const wStatus& AllocShm(char* ptr, size_t size = 0);
	const wStatus& Destroy();

protected:
	int mShmId;
	size_t mSize;
	std::string mFilename;
	struct Shmhead_t *mShmhead;
	wStatus mStatus;
};

}	// namespace hnet

#endif
