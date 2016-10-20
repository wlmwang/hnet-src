
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CHANNEL_TASK_H_
#define _W_CHANNEL_TASK_H_

#include "wCore.h"
#include "wStatus.h"
#include "wTask.h"

namespace hnet {

class wSocket;
class wWorker;

class wChannelTask : public wTask {
public:
    wChannelTask(wSocket *socket, wWorker *worker = NULL);

    int ChannelOpen(struct Request_t *request);
    int ChannelClose(struct Request_t *request);
    int ChannelQuit(struct Request_t *request);
    int ChannelTerminate(struct Request_t *request);
    
protected:
    wWorker *mWorker;
};

}	// namespace hnet

#endif
