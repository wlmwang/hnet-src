
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TCP_TASK_H_
#define _W_TCP_TASK_H_

#include "wCore.h"
#include "wCommand.h"
#include "wTask.h"

namespace hnet {

class wSocket;

class wTcpTask : public wTask {
public:
    wTcpTask(wSocket *socket, int32_t type = 0) : wTask(socket, type) { }
};

}	// namespace hnet

#endif
