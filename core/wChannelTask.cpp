
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wChannelTask.h" 
#include "wCommand.h"
#include "wMaster.h"
#include "wWorker.h"
#include "wChannelCmd.h"
#include "wSignal.h"

namespace hnet {

wChannelTask::wChannelTask(wSocket *socket, wWorker *worker) : wTask(socket), mWorker(worker) {
    RegisterFunc(CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen);
    RegisterFunc(CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose);
    RegisterFunc(CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit);
    RegisterFunc(CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate);
}

int wChannelTask::ChannelOpen(char *pBuffer, int iLen) {
	struct ChannelReqOpen_t *pCh = (struct ChannelReqOpen_t* )pBuffer;
	if (mWorker->mWorkerPool != NULL && mWorker->mWorkerPool[pCh->mSlot]) {
		LOG_DEBUG(ELOG_KEY, "[system] get channel solt:%i pid:%d fd:%d", pCh->mSlot, pCh->mPid, pCh->mFD);

		mWorker->mWorkerPool[pCh->mSlot]->mPid = pCh->mPid;
		mWorker->mWorkerPool[pCh->mSlot]->mCh[0] = pCh->mFD;
	}
	return 0;
}

int wChannelTask::ChannelClose(char *pBuffer, int iLen) {
	struct ChannelReqClose_t *pCh = (struct ChannelReqClose_t* )pBuffer;
	if (mWorker->mWorkerPool != NULL && mWorker->mWorkerPool[pCh->mSlot]) {
		LOG_DEBUG(ELOG_KEY, "[system] close channel s:%i pid:%d our:%d fd:%d", pCh->mSlot, pCh->mPid, 
			mWorker->mWorkerPool[pCh->mSlot]->mPid, mWorker->mWorkerPool[pCh->mSlot]->mCh[0]);
		
		if (close(mWorker->mWorkerPool[pCh->mSlot]->mCh[0]) == -1) {
			LOG_DEBUG(ELOG_KEY, "[system] close() channel failed: %s", strerror(errno));
		}
		mWorker->mWorkerPool[pCh->mSlot]->mCh[0] = FD_UNKNOWN; //-1
	}
	return 0;
}

int wChannelTask::ChannelQuit(char *pBuffer, int iLen) {
	g_quit = 1;
	return 0;
}

int wChannelTask::ChannelTerminate(char *pBuffer, int iLen) {
	g_terminate = 1;
	return 0;
}

}	// namespace hnet
