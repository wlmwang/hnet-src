
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wWorker.h"
#include "wSigSet.h"
//#include "wShm.h"
//#include "wShmtx.h"

wWorker::wWorker(int iSlot) : mSlot(iSlot)
{
	//worker通信 主要通过channel同步个fd，填充进程表
	mIpc = new wWorkerIpc(this);
}

wWorker::~wWorker()
{
	CloseChannel();
	SAFE_DELETE(mIpc);
}

int wWorker::OpenChannel()
{
	return mCh.Open();
}

void wWorker::CloseChannel()
{
	mCh.Close();
}

void wWorker::PrepareStart(int iSlot, int iType, string sTitle, void **pData) 
{
	mPid = getpid();
	mSlot = iSlot;
	mRespawn = iType;
	mName = sTitle;
	mWorkerNum = *(int*)pData[0];
	mWorkerPool = (wWorker**)pData[1];

	/**
	 *  设置当前进程优先级。进程默认优先级为0
	 *  -20 -> 20 高 -> 低。只有root可提高优先级，即可减少priority值
	 */
	if (mSlot >= 0 && mPriority != 0)
	{
        if (setpriority(PRIO_PROCESS, 0, mPriority) == -1) 
		{
			mErr = errno;
			LOG_ERROR(ELOG_KEY, "[system] worker process setpriority(%d) failed: %s", mPriority, strerror(mErr));
        }
    }
	
	/**
	 *  设置进程的最大文件描述符
	 */
    if (mRlimitCore != -1) 
	{
		struct rlimit rlmt;
        rlmt.rlim_cur = (rlim_t) mRlimitCore;
        rlmt.rlim_max = (rlim_t) mRlimitCore;
        if (setrlimit(RLIMIT_NOFILE, &rlmt) == -1) 
		{
			mErr = errno;
			LOG_ERROR(ELOG_KEY, "[system] worker process setrlimit(RLIMIT_NOFILE, %i) failed: %s", mRlimitCore, strerror(mErr));
        }
    }
	
	srandom((mPid << 16) ^ time(NULL));  //设置种子值，进程ID+时间
	
	//将其他进程的channel[1]关闭，自己的除外
    for (int n = 0; n < mWorkerNum; n++) 
    {
    	LOG_DEBUG(ELOG_KEY, "[system] pid:%d,workernum:%d, n:%d,mch:%d", mPid, mWorkerNum, n, mWorkerPool[n]->mCh[1]);
        if (n == mSlot || mWorkerPool[n]->mPid == -1|| mWorkerPool[n]->mCh[1] == FD_UNKNOWN) 
        {
            continue;
        }

        if (close(mWorkerPool[n]->mCh[1]) == -1) 
        {
        	mErr = errno;
            LOG_ERROR(ELOG_KEY, "[system] worker process close() channel failed: %s", strerror(mErr));
        }
    }

    //关闭该进程worker进程的ch[0]描述符
    if (close(mWorkerPool[mSlot]->mCh[0]) == -1) 
    {
    	mErr = errno;
        LOG_ERROR(ELOG_KEY, "[system] worker process close() channel failed: %s", strerror(mErr));
    }
	
	//worker进程中不阻塞所有信号（恢复信号处理）
	wSigSet mSigSet;
	if (mSigSet.Procmask(SIG_SETMASK))
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] worker process sigprocmask() failed: %s", strerror(mErr));
	}
	
	PrepareRun();
}

void wWorker::Start(bool bDaemon) 
{
	mStatus = WORKER_RUNNING;
	
	mIpc->StartThread();
	Run();
}
