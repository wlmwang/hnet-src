
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wChannelTask.h"
#include "wMaster.h"
#include "wChannelCmd.h"
#include "wSignal.h"

wChannelTask::wChannelTask(wSocket *pSocket, wWorker *pWorker) : wTask(pSocket), mWorker(pWorker)
{
	REG_DISP(mDispatch, "wChannelTask", CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen);
	REG_DISP(mDispatch, "wChannelTask", CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose);
	REG_DISP(mDispatch, "wChannelTask", CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit);
	REG_DISP(mDispatch, "wChannelTask", CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate);
}

int wChannelTask::HandleRecvMessage(char *pBuffer, int nLen)
{
	W_ASSERT(pBuffer != NULL, return -1);

	struct wCommand *pCommand = (struct wCommand*) pBuffer;
	return ParseRecvMessage(pCommand, pBuffer, nLen);
}

int wChannelTask::ParseRecvMessage(struct wCommand* pCommand, char *pBuffer, int iLen)
{
	if (pCommand->GetId() == W_CMD(CMD_NULL, PARA_NULL))
	{
		//空消息(心跳返回)
		mHeartbeatTimes = 0;
	}
	else
	{
		auto pF = mDispatch.GetFuncT("wChannelTask", pCommand->GetId());
		if (pF != NULL)
		{
			pF->mFunc(pBuffer, iLen);
		}
		else
		{
			LOG_ERROR(ELOG_KEY, "[system] client send a invalid msg fd(%d) id(%d)", mSocket->FD(), pCommand->GetId());
		}
	}
	return 0;
}

int wChannelTask::ChannelOpen(char *pBuffer, int iLen)
{
	W_ASSERT(mWorker != NULL, return -1);

	struct ChannelReqOpen_t *pCh = (struct ChannelReqOpen_t* )pBuffer;
	if (mWorker->mWorkerPool != NULL && mWorker->mWorkerPool[pCh->mSlot])
	{
		LOG_DEBUG(ELOG_KEY, "[system] get channel solt:%i pid:%d fd:%d", pCh->mSlot, pCh->mPid, pCh->mFD);

		mWorker->mWorkerPool[pCh->mSlot]->mPid = pCh->mPid;
		mWorker->mWorkerPool[pCh->mSlot]->mCh[0] = pCh->mFD;
	}
	return 0;
}

int wChannelTask::ChannelClose(char *pBuffer, int iLen)
{
	W_ASSERT(mWorker != NULL, return -1);

	struct ChannelReqClose_t *pCh = (struct ChannelReqClose_t* )pBuffer;
	if (mWorker->mWorkerPool != NULL && mWorker->mWorkerPool[pCh->mSlot])
	{
		LOG_DEBUG(ELOG_KEY, "[system] close channel s:%i pid:%d our:%d fd:%d", pCh->mSlot, pCh->mPid, 
			mWorker->mWorkerPool[pCh->mSlot]->mPid, mWorker->mWorkerPool[pCh->mSlot]->mCh[0]);
		
		if (close(mWorker->mWorkerPool[pCh->mSlot]->mCh[0]) == -1) 
		{
			LOG_DEBUG(ELOG_KEY, "[system] close() channel failed: %s", strerror(errno));
		}
		mWorker->mWorkerPool[pCh->mSlot]->mCh[0] = FD_UNKNOWN; //-1
	}
	return 0;
}

int wChannelTask::ChannelQuit(char *pBuffer, int iLen)
{
	g_quit = 1;
	return 0;
}

int wChannelTask::ChannelTerminate(char *pBuffer, int iLen)
{
	g_terminate = 1;
	return 0;
}
