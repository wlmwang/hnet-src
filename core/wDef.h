
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

//socket errno
#define ERR_NOBUFF -96    //socket buf写满 
#define ERR_TIMEOUT -97	//connect连接超时
#define ERR_MSGLEN -98	//接受消息长度不合法
#define ERR_CLOSED -99	//socket对端close

//进程相关
#define MAX_PROCESSES         1024

#define PROCESS_SINGLE     0 	//单独进程
#define PROCESS_MASTER     1 	//主进程
#define PROCESS_SIGNALLER  2 	//信号进程
#define PROCESS_WORKER     3 	//工作进程

#define PROCESS_NORESPAWN     -1    //子进程退出时，父进程不再创建
#define PROCESS_JUST_SPAWN    -2    //子进程正在重启，该进程创建之后，再次退出时，父进程不再创建
#define PROCESS_RESPAWN       -3    //子进程异常退出时，父进程会重新创建它
#define PROCESS_JUST_RESPAWN  -4    //子进程正在重启，该进程创建之后，再次退出时，父进程会重新创建它
#define PROCESS_DETACHED      -5    //分离进程（热代码替换）

#endif
