
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TCP_TASK_H_
#define _W_TCP_TASK_H_

#include "wCore.h"
#include "wCommand.h"
#include "wTask.h"

class wTcpTask : public wTask
{
	public:
		wTcpTask() {}
		wTcpTask(wSocket *pSocket) : wTask(pSocket) {}

		virtual int VerifyConn() { return 0;}
		virtual int Verify() { return 0;}
};

#endif
