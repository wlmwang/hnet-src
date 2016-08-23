
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wShmtx.h"

int wShmtx::Create(wShm *pShm, int iSpin)
{
	char *pAddr = pShm->AllocShm(sizeof(wSem));
	if (pAddr == NULL)
	{
		LOG_ERROR(ELOG_KEY, "[system] shm alloc failed for shmtx: %d", sizeof(wSem));
		return -1;
	}
	mSem = (wSem *) pAddr;
	mSpin = iSpin;
	return mSem->Initialize();
}

int wShmtx::Lock()
{
	if (mSem == NULL)
	{
		return -1;
	}
	return mSem->Wait();
}

int wShmtx::Unlock()
{
	if (mSem == NULL)
	{
		return -1;
	}
	return mSem->Post();
}

int wShmtx::TryLock()
{
	if (mSem == NULL)
	{
		return -1;
	}
	return mSem->TryWait();
}

void wShmtx::LockSpin()
{
    int	i, n;

    int ncpu = sysconf(_SC_NPROCESSORS_ONLN);   //cpu个数

    while (true) 
    {
        if (mSem->TryWait() == 0)
        {
        	return;
        }

        if (ncpu > 1) 
        {
            for (n = 1; n < mSpin; n <<= 1) 
            {
                for (i = 0; i < n; i++) 
                {
                    pause();	//暂停
                }

		        if (mSem->TryWait() == 0)
		        {
		        	return;
		        }
            }
        }
        sched_yield();	//usleep(1);
    }
}
