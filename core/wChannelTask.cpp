
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wChannelTask.h" 
#include "wMisc.h"
#include "wLogger.h"
#include "wCommand.h"
#include "wMaster.h"
#include "wWorker.h"
#include "wSignal.h"

#ifdef _USE_PROTOBUF_
#include "wChannel.pb.h"
#else
#include "wChannelCmd.h"
#endif

namespace hnet {

wChannelTask::wChannelTask(wSocket *socket, wMaster *master, int32_t type) : wTask(socket, type), mMaster(master) {
#ifdef _USE_PROTOBUF_
	On("hnet.wChannelOpen", &wChannelTask::ChannelOpen, this);
	On("hnet.wChannelClose", &wChannelTask::ChannelClose, this);
	On("hnet.wChannelQuit", &wChannelTask::ChannelQuit, this);
	On("hnet.wChannelTerminate", &wChannelTask::ChannelTerminate, this);
#else
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate, this);
#endif
}

int wChannelTask::ChannelOpen(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	wChannelOpen open;
#else
	wChannelReqOpen_t open;
#endif
	open.ParseFromArray(request->mBuf, request->mLen);
	mMaster->Worker(open.slot())->Pid() = open.pid();
	mMaster->Worker(open.slot())->ChannelFD(0) = open.fd();
	return 0;
}

int wChannelTask::ChannelClose(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	wChannelClose cls;
#else
	wChannelReqClose_t cls;
#endif
	cls.ParseFromArray(request->mBuf, request->mLen);
	if (mMaster->Worker(cls.slot())->ChannelFD(0) != kFDUnknown) {
		if (close(mMaster->Worker(cls.slot())->ChannelFD(0)) == -1) {
	    	LOG_DEBUG(soft::GetLogPath(), "%s : %s", "wChannelTask::ChannelClose, close() failed", error::Strerror(errno).c_str());
		}
		mMaster->Worker(cls.slot())->ChannelFD(0) = kFDUnknown;
	}
	mMaster->Worker(cls.slot())->Pid() = -1;
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
