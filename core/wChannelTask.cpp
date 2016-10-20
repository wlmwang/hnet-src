
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
	AddHandler(CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen, this);
	AddHandler(CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose, this);
	AddHandler(CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit, this);
	AddHandler(CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate, this);
}

int wChannelTask::ChannelOpen(struct Message_t *msg) {
	struct ChannelReqOpen_t* ch = reinterpret_cast<struct ChannelReqOpen_t*>(msg->mBuf);
	if (mWorker->Master()->Worker(ch->mSlot) != NULL) {
		mWorker->Master()->Worker(ch->mSlot)->Pid() = ch->mPid;
		mWorker->Master()->Worker(ch->mSlot)->ChannelFD(0) = ch->mPid;
	}
	return 0;
}

int wChannelTask::ChannelClose(struct Message_t *msg) {
	struct ChannelReqClose_t* ch = reinterpret_cast<struct ChannelReqClose_t*>(msg->mBuf);
	if (mWorker->Master()->Worker(ch->mSlot) != NULL) {
		if (close(mWorker->Master()->Worker(ch->mSlot)->ChannelFD(0)) == -1) {
			//wStatus::IOError("wChannelTask::ChannelClose, close() failed", strerror(errno));
		}
		mWorker->Master()->Worker(ch->mSlot)->ChannelFD(0) = kFDUnknown;
	}
	return 0;
}

int wChannelTask::ChannelQuit(struct Message_t *msg) {
	g_quit = 1;
	return 0;
}

int wChannelTask::ChannelTerminate(struct Message_t *msg) {
	g_terminate = 1;
	return 0;
}

}	// namespace hnet
