
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
	REG_DISP(mDispatch, Name(), CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen);
	REG_DISP(mDispatch, Name(), CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose);
	REG_DISP(mDispatch, Name(), CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit);
	REG_DISP(mDispatch, Name(), CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate);
}

int wChannelTask::ChannelOpen(char buf[], uint32_t len) {
	struct ChannelReqOpen_t* ch = reinterpret_cast<struct ChannelReqOpen_t*>(buf);
	if (mWorker->Master()->Worker(ch->mSlot) != NULL) {
		mWorker->Master()->Worker(ch->mSlot)->Pid() = ch->mPid;
		mWorker->Master()->Worker(ch->mSlot)->ChannelFD(0) = ch->mPid;
	}
	return 0;
}

int wChannelTask::ChannelClose(char *buf, uint32_t len) {
	struct ChannelReqClose_t* ch = reinterpret_cast<struct ChannelReqClose_t*>(buf);
	if (mWorker->Master()->Worker(ch->mSlot) != NULL) {
		if (close(mWorker->Master()->Worker(ch->mSlot)->ChannelFD(0)) == -1) {
			//wStatus::IOError("wChannelTask::ChannelClose, close() failed", strerror(errno));
		}
		mWorker->Master()->Worker(ch->mSlot)->ChannelFD(0) = kFDUnknown;
	}
	return 0;
}

int wChannelTask::ChannelQuit(char *buf, uint32_t len) {
	g_quit = 1;
	return 0;
}

int wChannelTask::ChannelTerminate(char *buf, uint32_t len) {
	g_terminate = 1;
	return 0;
}

}	// namespace hnet
