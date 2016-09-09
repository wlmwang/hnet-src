
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_DEF_H_
#define _W_DEF_H_


enum MASTER_STATUS
{
    MASTER_INIT = -1,
    MASTER_RUNNING,
    MASTER_HUP,
    MASTER_EXITING,
    MASTER_EXITED
};

enum WORKER_STATUS
{
    WORKER_INIT = -1,
    WORKER_RUNNING,
    WORKER_HUP,
    WORKER_EXITING,
    WORKER_EXITED
};

enum SERVER_STATUS
{
    SERVER_INIT = -1,
    SERVER_QUIT,
    SERVER_RUNNING
};

enum CLIENT_STATUS
{
    CLIENT_INIT = -1,
    CLIENT_QUIT,    
    CLIENT_RUNNING
};

enum TASK_STATUS
{
    TASK_INIT = -1,
    TASK_QUIT,
    TASK_RUNNING
};

#endif
