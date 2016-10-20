
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_UNIX_TASK_H_
#define _W_UNIX_TASK_H_

#include "wCore.h"
#include "wCommand.h"
#include "wTask.h"

namespace hnet {

class wUnixTask : public wTask {
public:
    wUnixTask(wSocket *socket, int32_t type = 0) : wTask(socket, type) { }
};

}	// namespace hnet

#endif
