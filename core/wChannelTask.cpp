
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
#include "wChannelCmd.h"

namespace hnet {

wChannelTask::wChannelTask(wSocket *socket, wMaster *master, int32_t type) : wTask(socket, type), mMaster(master) {
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, &wChannelTask::ChannelOpen, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_CLOSE, &wChannelTask::ChannelClose, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_QUIT, &wChannelTask::ChannelQuit, this);
	On(CMD_CHANNEL_REQ, CHANNEL_REQ_TERMINATE, &wChannelTask::ChannelTerminate, this);
}

int wChannelTask::ChannelOpen(struct Request_t *request) {
	wChannelReqOpen_t open;
	open.ParseFromArray(request->mBuf, request->mLen);

	// 更新描述符
	mMaster->Worker(open.slot())->Pid() = open.pid();
	mMaster->Worker(open.slot())->ChannelFD(0) = open.fd();
	mMaster->Worker(open.slot())->Timeline() = soft::TimeUnix();

	// 添加task对象，预备channel的异步写事件
	wChannelSocket *socket = mMaster->Worker(open.slot())->Channel();
	if (socket) {
		socket->FD() = open.fd();
		socket->SS() = kSsConnected;

		wTask *task;
		if (mMaster->Server()->NewChannelTask(socket, &task).Ok() && task) {
			mMaster->Server()->AddTask(task);
		}
	}
	return 0;
}

int wChannelTask::ChannelClose(struct Request_t *request) {
	wChannelReqClose_t cls;
	cls.ParseFromArray(request->mBuf, request->mLen);
	
	// 移除事件及task对象
	wChannelSocket *socket = mMaster->Worker(cls.slot())->Channel();
	if (socket) {
		wTask *task;
		if (mMaster->Server()->FindTaskBySocket(&task, socket).Ok() && task) {
			mMaster->Server()->RemoveTask(task);	// 移除task队列
		}
	}

	// 关闭描述符
	if (mMaster->Worker(cls.slot())->ChannelFD(0) != kFDUnknown) {
		if (close(mMaster->Worker(cls.slot())->ChannelFD(0)) == -1) {
	    	LOG_DEBUG(soft::GetLogPath(), "%s : %s", "wChannelTask::ChannelClose, close() failed", error::Strerror(errno).c_str());
		}
	}
	mMaster->Worker(cls.slot())->ChannelFD(0) = kFDUnknown;
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
