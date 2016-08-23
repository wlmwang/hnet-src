
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CHANNEL_TASK_H_
#define _W_CHANNEL_TASK_H_

#include "wCore.h"
#include "wAssert.h"
#include "wLog.h" 
#include "wCommand.h"
#include "wDispatch.h"
#include "wTask.h"
#include "wWorker.h"

class wChannelTask : public wTask
{
	public:
		wChannelTask(wSocket *pSocket, wWorker *pWorker = NULL);

		virtual int HandleRecvMessage(char *pBuffer, int nLen);
		int ParseRecvMessage(struct wCommand* pCommand, char *pBuffer, int iLen);

		DEC_FUNC(ChannelOpen);
		DEC_FUNC(ChannelClose);
		DEC_FUNC(ChannelQuit);
		DEC_FUNC(ChannelTerminate);

	protected:
		wWorker *mWorker {NULL};
		DEC_DISP(mDispatch);
};

#endif
