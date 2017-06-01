
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SHM_H_
#define _W_SHM_H_

#include <sys/ipc.h>
#include <sys/shm.h>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

// 共享内存存储头信息Shmhead_t结构体
struct Shmhead_t {
	// 共享内存起始地址
	uintptr_t mStart;

	// 共享内存结束地址
	uintptr_t mEnd;

	// 已被分配共享内存偏移地址offset，即下次可使用的开始地址
	uintptr_t mUsedOff;
};

// 共享内存管理
class wShm : private wNoncopyable {
public:
	virtual ~wShm() { }

	// create创建shm
	virtual int CreateShm(int pipeid = 'i') = 0;
	
	// attached已创建的shm
	virtual int AttachShm(int pipeid = 'i') = 0;
	
	// alloc创建的shm
	virtual void* AllocShm(size_t size = 0) = 0;
	
	// 关闭当前进程中的共享内存句柄
	virtual void Remove() = 0;

	// 删除系统中的shm
	virtual void Destroy() = 0;
};

// 共享内存实现类
class wPosixShm : public wShm {
public:
	wPosixShm(const std::string& filename, size_t size = kMsgQueueLen);
	virtual ~wPosixShm();

	virtual int CreateShm(int pipeid = 'i');
	virtual int AttachShm(int pipeid = 'i');
	virtual void* AllocShm(size_t size = 0);
	virtual void Remove();
	virtual void Destroy();

protected:
	int mShmId;
	size_t mSize;
	struct Shmhead_t *mShmhead;
	std::string mFilename;
};

}	// namespace hnet

#endif
