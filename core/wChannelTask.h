
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

    int ChannelOpen(struct Message_t *msg);
    int ChannelClose(struct Message_t *msg);
    int ChannelQuit(struct Message_t *msg);
    int ChannelTerminate(struct Message_t *msg);
    
protected:
    wWorker *mWorker;
};

}	// namespace hnet

#endif
