
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wChannelTask.h" 
#include "wCommand.h"
#include "wMaster.h"
#include "wWorker.h"
#include "wSignal.h"
#include "wChannel.pb.h"

namespace hnet {

wChannelTask::wChannelTask(wSocket *socket, wMaster *master, int32_t type) : wTask(socket, type), mMaster(master) {
	On("hnet.wChannelOpen", &wChannelTask::ChannelOpen, this);
	On("hnet.wChannelClose", &wChannelTask::ChannelClose, this);
	On("hnet.wChannelQuit", &wChannelTask::ChannelQuit, this);
	On("hnet.wChannelTerminate", &wChannelTask::ChannelTerminate, this);
}

int wChannelTask::ChannelOpen(struct Request_t *request) {
	wChannelOpen open;
	open.ParseFromArray(request->mBuf, request->mLen);
	mMaster->Worker(open.slot())->Pid() = open.pid();
	mMaster->Worker(open.slot())->ChannelFD(0) = open.fd();
	return 0;
}

int wChannelTask::ChannelClose(struct Request_t *request) {
	wChannelClose cls;
	cls.ParseFromArray(request->mBuf, request->mLen);
	if (mMaster->Worker(cls.slot())->ChannelFD(0) != kFDUnknown) {
		if (close(mMaster->Worker(cls.slot())->ChannelFD(0)) == -1) {
			wStatus::IOError("wChannelTask::ChannelClose, close() failed", strerror(errno));
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
