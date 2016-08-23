
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_DEF_H_
#define _W_DEF_H_

//项目名 版本
#define SOFTWARE_NAME   "hlbs"
#define SOFTWARE_VER    "2.2"

//主机名长度
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  256
#endif

//换行
#define LF (u_char)     '\n'
#define CR (u_char)     '\r'
#define CRLF            "\r\n"
#define SETPROCTITLE_PAD '\0'

//INT IP长度
#define INT32_LEN   (sizeof("-2147483648") - 1)
#define INT64_LEN   (sizeof("-9223372036854775808") - 1)
#define MAX_IP_LEN  16

//keep-alive 间隔 次数
#define KEEPALIVE_TIME  3000
#define KEEPALIVE_CNT   5

//FD epoll backlog
#define FD_UNKNOWN	-1
#define LISTEN_BACKLOG	511

//内存池大小
#define MEM_POOL_LEN 16777216 /* 16M */

//shm消息队列大小
#define MSG_QUEUE_LEN 16777216 /* 16M */

//消息缓冲大小
#define MSG_BUFF_LEN 524288 /* 512k */

//封包限制
#define MAX_PACKAGE_LEN 524284  /* 512k-4 */
#define MIN_PACKAGE_LEN 1

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

#define PID_PATH		"../log/master.pid"
#define LOCK_PATH		"../log/master.lock"
#define ACCEPT_MUTEX	"../log/accept.mutex"	//worker惊群锁

//#define USER  "nobody"
//#define GROUP  "nobody"

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
