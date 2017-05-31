
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CORE_H_
#define _W_CORE_H_

#include "wLinux.h"

#include <cassert>
#include <cerrno>
#include <climits>
#include <ctime>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdio>

#define SAFE_NEW(type, ptr) \
do { \
	try { \
		ptr = NULL; \
		ptr = new type; \
	} catch (...) { \
		ptr = NULL; \
	} \
} while (0)

#define SAFE_NEW_VEC(n, type, ptr) \
do { \
	try { \
		ptr = NULL; \
		ptr = new type[n]; \
	} catch (...) { \
		ptr = NULL; \
	} \
} while (0)

#define SAFE_DELETE(ptr) \
do { \
	if (ptr) { \
		delete ptr; \
		ptr = NULL; \
	} \
} while (0)

#define SAFE_DELETE_VEC(ptr) \
do { \
	if (ptr) { \
		delete[] ptr; \
		ptr = NULL; \
	} \
} while (0)

namespace hnet {

const char      kProcTitlePad = '\0';
const char      kLF = '\n';
const char      kCR = '\r';
const char      kCRLF[] = "\r\n";

const uint32_t	kMaxErrorLen = 255;
const uint32_t  kMaxHostNameLen = 255;
const uint8_t   kMaxIpLen = 16;

const uint32_t  kListenBacklog = 256;
const int32_t   kFDUnknown = -1;

const uint32_t  kKeepAliveTm = 3000;
const uint8_t   kKeepAliveCnt = 10;

// 100M单个归档日志大小
const off_t		kMaxLoggerSize = 104857600;
const uint8_t	kLoggerNum = 16;

// 1M共享消息队列大小
const uint32_t  kMsgQueueLen = 1048576;

// 64k客户端task消息缓冲大小
const uint32_t  kPackageSize = 65536;
const uint32_t  kMaxPackageSize = 65532;
const uint32_t  kMinPackageSize = 3;

const uint32_t  kPageSize = 4096;
const bool		kLittleEndian = true;

// 心跳线程与主线程分离开关
const bool		kScheduleTurn = false;
// 心跳开关
const bool		kHeartbeatTurn = true;
const uint8_t   kHeartbeat = 10;

// 惊群锁开关
const bool		kAcceptTurn = true;
// 惊群锁实现可使用以下三种
// 0:atmoic（原子锁，共享内存支持） 
// 1:semaphore（信号量，sem_open支持。注意：跨进程极其不稳定） 
// 2:file（记录锁，多数POSIX平台均对flock支持）
const int8_t	kAcceptStuff = 0;
const char		kAcceptMutex[] = "accept.lock";

// 消息协议
const int8_t	kMpCommand = 1;
const int8_t	kMpProtobuf = 2;

// 进程相关
const uint32_t	kMaxProcess = 1024;
const int8_t    kProcessNoRespawn = -1;		// 子进程退出时，父进程不再创建
const int8_t    kProcessJustSpawn = -2;		// 子进程正在重启，该进程创建之后，再次退出时，父进程不再创建
const int8_t    kProcessRespawn = -3;     	// 子进程异常退出时，父进程会重新创建它
const int8_t    kProcessJustRespawn = -4;	// 子进程正在重启，该进程创建之后，再次退出时，父进程会重新创建它
const int8_t    kProcessDetached = -5;		// 分离进程

// 项目相关
const uid_t     kDeamonUser = 0;
const gid_t     kDeamonGroup = 0;

const char      kSoftwareName[]   = "HNET";
const char      kSoftwareVer[]    = "0.0.15";

const char		kBinPath[] = "./";
const char      kLockPath[] = "hnet.lock";
const char      kPidPath[] = "hnet.pid";
const char      kLogPath[] = "hnet.log";

}   // namespace hnet

#endif
