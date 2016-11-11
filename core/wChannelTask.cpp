
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

wChannelTask::wChannelTask(wSocket *socket, wMaster *master, int32_t type) : wTask(socket, type), mMaster(master) {
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate, this);
}

int wChannelTask::ChannelOpen(struct Request_t *request) {
	struct ChannelReqOpen_t* ch = reinterpret_cast<struct ChannelReqOpen_t*>(request->mBuf);
	if (mMaster->Worker(ch->mSlot) != NULL) {
		mMaster->Worker(ch->mSlot)->Pid() = ch->mPid;
		mMaster->Worker(ch->mSlot)->ChannelFD(0) = ch->mPid;
	}
	return 0;
}

int wChannelTask::ChannelClose(struct Request_t *request) {
	struct ChannelReqClose_t* ch = reinterpret_cast<struct ChannelReqClose_t*>(request->mBuf);
	if (mMaster->Worker(ch->mSlot) != NULL && mMaster->Worker(ch->mSlot)->ChannelFD(0) != kFDUnknown) {
		if (close(mMaster->Worker(ch->mSlot)->ChannelFD(0)) == -1) {
			wStatus::IOError("wChannelTask::ChannelClose, close() failed", strerror(errno));
		}
		mMaster->Worker(ch->mSlot)->ChannelFD(0) = kFDUnknown;
	}
	return 0;
}

int wChannelTask::ChannelQuit(struct Request_t *request) {
	g_quit = 1;
	return 0;
}

int wChannelTask::ChannelTerminate(struct Request_t *request) {
	g_terminate = 1;
	return 0;
}

}	// namespace hnet
