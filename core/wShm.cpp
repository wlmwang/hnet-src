
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */
 
#include "wShm.h"

wShm::wShm(const char *filename, int pipeid, size_t size)
{
	mPagesize = getpagesize();

	mPipeId = pipeid;
	mSize = size + sizeof(struct shmhead_t);
	if(mPagesize > 0)
	{
		mSize = ALIGN(mSize, mPagesize);
	}
	
	memcpy(mFilename, filename, strlen(filename) +1);
}

wShm::~wShm()
{
	FreeShm();
}

char *wShm::CreateShm()
{
	LOG_DEBUG(ELOG_KEY, "[system] try to alloc %lld bytes of share memory", mSize);
	
	int iFD = open(mFilename, O_CREAT);
	if (iFD < 0)
	{
		LOG_ERROR(ELOG_KEY, "[system] open file(%s) failed: %s", mFilename, strerror(errno));
		return NULL;
	}
	close(iFD);
	//unlink(mFilename);

	mKey = ftok(mFilename, mPipeId);
	if (mKey < 0) 
	{
		LOG_ERROR(ELOG_KEY, "[system] create memory (ftok) failed: %s", strerror(errno));
		return NULL;
	}

	//申请共享内存
	mShmId = shmget(mKey, mSize, IPC_CREAT| IPC_EXCL| 0666);
	
	//如果申请内存失败
	if (mShmId < 0) 
	{
		if (errno != EEXIST) 
		{
			LOG_ERROR(ELOG_KEY, "[system] alloc share memory failed: %s", strerror(errno));
			return 0;
		}

		LOG_DEBUG(ELOG_KEY, "[system] share memory is exist now, try to attach it");

		//如果该内存已经被申请，则申请访问控制它
		mShmId = shmget(mKey, mSize, 0666);

		//如果失败
		if (mShmId < 0) 
		{
			LOG_DEBUG(ELOG_KEY, "[system] attach to share memory failed: %s, try to touch it", strerror(errno));
			
			//猜测是否是该内存大小太小，先获取内存ID
			mShmId = shmget(mKey, 0, 0666);
			
			//如果失败，则无法操作该内存，只能退出
			if (mShmId < 0) 
			{
				LOG_ERROR(ELOG_KEY, "[system] touch to share memory failed: %s", strerror(errno));
				return 0;
			}
			else 
			{
				LOG_DEBUG(ELOG_KEY, "[system] remove the exist share memory %d", mShmId);

				//如果成功，则先删除原内存
				if (shmctl(mShmId, IPC_RMID, NULL) < 0) 
				{
					LOG_ERROR(ELOG_KEY, "[system] remove share memory failed: %s", strerror(errno));
					return 0;
				}

				//再次申请该ID的内存
				mShmId = shmget(mKey, mSize, IPC_CREAT|IPC_EXCL|0666);
				if (mShmId < 0) 
				{
					LOG_ERROR(ELOG_KEY, "[system] alloc share memory failed again: %s", strerror(errno));
					return 0;
				}
			}
		}
		else
		{
			LOG_DEBUG(ELOG_KEY, "[system] attach to share memory succeed");
		}
	}

	LOG_DEBUG(ELOG_KEY, "[system] alloc %lld bytes of share memory succeed", mSize);
	
	char *pAddr = (char *)shmat(mShmId, NULL, 0);
    if (pAddr == (char *)-1) 
	{
		LOG_ERROR(ELOG_KEY, "[system] shmat failed: %s", strerror(errno));
		return 0;
    }

    //shm头
	mShmhead = (struct shmhead_t*) pAddr;
	mShmhead->mStart = pAddr;
	mShmhead->mEnd = pAddr + mSize;
	mShmhead->mUsedOff = pAddr + sizeof(struct shmhead_t);
	return mShmhead->mUsedOff;
}

char *wShm::AttachShm()
{
	LOG_DEBUG(ELOG_KEY, "[system] try to attach %lld bytes of share memory", mSize);
	
	//把需要申请共享内存的key值申请出来
	mKey = ftok(mFilename, mPipeId);
	if (mKey < 0) 
	{
		LOG_ERROR(ELOG_KEY, "[system] create memory (ftok) failed: %s", strerror(errno));
		return 0;
	}

	// 尝试获取
	int mShmId = shmget(mKey, mSize, 0666);
	if(mShmId < 0) 
	{
		LOG_ERROR(ELOG_KEY, "[system] attach to share memory failed: %s", strerror(errno));
		return 0;
	}
	
	char *pAddr = (char *)shmat(mShmId, NULL, 0);
    if (pAddr == (char *) -1) 
	{
		LOG_ERROR(ELOG_KEY, "[system] shmat() failed: %s", strerror(errno));
		return 0;
    }
	
    //shm头
	mShmhead = (struct shmhead_t*) pAddr;
	return mShmhead->mUsedOff;
}

char *wShm::AllocShm(size_t size)
{
	if (mShmhead == NULL)
	{
		LOG_ERROR(ELOG_KEY, "[system] shm need exec function CreateShm or AttachShm first");
		return NULL;
	}

	if(mShmhead != NULL && mShmhead->mUsedOff + size < mShmhead->mEnd)
	{
		char *pAddr = mShmhead->mUsedOff;
		mShmhead->mUsedOff += size;
		memset(pAddr, 0, size);
		return pAddr;
	}
	LOG_ERROR(ELOG_KEY, "[system] alloc(%d) shm failed,shm space not enough, real free(%d)", size, mShmhead->mEnd - mShmhead->mUsedOff);
	return NULL;
}

void wShm::FreeShm()
{
	LOG_DEBUG(ELOG_KEY, "[system] free %lld bytes of share memory", mSize);
	
	if(mShmhead == NULL || mShmhead->mStart == NULL)
	{
		LOG_ERROR(ELOG_KEY, "[system] free shm failed: shm head illegal");
		return;
	}

	//对共享操作结束，分离该shmid_ds与该进程关联计数器
    if (shmdt(mShmhead->mStart) == -1)
	{
		LOG_ERROR(ELOG_KEY, "[system] shmdt(%d) failed", mShmhead->mStart);
    }
	
	//删除该shmid_ds共享存储段（全部进程结束才会真正删除）
    if (shmctl(mShmId, IPC_RMID, NULL) == -1)
	{
		LOG_ERROR(ELOG_KEY, "[system] remove share memory failed: %s", strerror(errno));
    }
	//unlink(mFilename);
}
