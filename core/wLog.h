
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_LOG_H_
#define _W_LOG_H_

#include <cstdarg>
#include "wCore.h"

// 已弃用（wLogger代替）

// 基于log4cpp的日志
namespace hnet {

const char	    kErrLogKey[] = "error";
const char	    kErrLogFile[] = "log/error.log";
const uint32_t  kErrLogSize = 10 << 20;
const uint32_t	kErrLogBackup = 20;

//日志等级
//NOTSET <  DEBUG < INFO  < WARN < LEVEL_NOTICE < ERROR  < FATAL 
enum LogLevel {	
    LEVEL_FATAL  = 0,
    LEVEL_ERROR  = 300,
    LEVEL_WARN   = 400,
    LEVEL_NOTICE = 500,
    LEVEL_INFO   = 600,
    LEVEL_DEBUG  = 700,
    LEVEL_NOTSET = 800,
};

//日志等级
//NOTSET <  DEBUG < INFO  < WARN < LEVEL_NOTICE < ERROR  < FATAL 
enum {
    kLevelFatal = 0,
    kLevelError = 300,
    kLevelWarn  = 400,
    kLevelNotice= 500,
    kLevelInfo  = 600,
    kLevelDebug = 700,
    kLevelNotset= 800
};

// 日志系统开关
#ifdef USE_LOG4CPP
#   define  INIT_ROLLINGFILE_LOG(vLogName, vLogDir, vPriority, ...) InitLog(vLogName, vLogDir, vPriority, ##__VA_ARGS__)
#   define  RE_INIT_LOG(vLogName, vPriority, ...)                   ReInitLog(vLogName, vPriority, ##__VA_ARGS__)
#   define  LOG(vLogName, vPriority, vFmt, ...)                     Log(vLogName, vPriority, vFmt, ##__VA_ARGS__)

#ifdef _DEBUG_
#   define  LOG_DEBUG(vLogName, vFmt, ...)	LogDebug(vLogName, vFmt, ##__VA_ARGS__)
#else
#   define  LOG_DEBUG(vLogName, vFmt, ...)
#endif

#   define  LOG_NOTICE(vLogName, vFmt, ...)	LogNotice(vLogName, vFmt, ##__VA_ARGS__)
#   define  LOG_INFO(vLogName, vFmt, ...)	LogInfo(vLogName, vFmt, ##__VA_ARGS__)
#   define  LOG_WARN(vLogName, vFmt, ...)	LogWarn(vLogName, vFmt, ##__VA_ARGS__)
#   define  LOG_ERROR(vLogName, vFmt, ...)	LogError(vLogName, vFmt, ##__VA_ARGS__)
#   define  LOG_FATAL(vLogName, vFmt, ...)	LogFatal(vLogName, vFmt, ##__VA_ARGS__)
#   define  LOG_SHUTDOWN_ALL()              ShutdownAllLog()

#else
#   define  INIT_ROLLINGFILE_LOG(vLogName, vLogDir, vPriority, ...)
#   define  RE_INIT_LOG(vLogName, vPriority, ...)
#   define  LOG(vLogName, vPriority, vFmt, ...)
#   define  LOG_DEBUG(vLogName, vFmt, ...)
#   define  LOG_NOTICE(vLogName, vFmt, ...)
#   define  LOG_INFO(vLogName, vFmt, ...)
#   define  LOG_WARN(vLogName, vFmt, ...)
#   define  LOG_ERROR(vLogName, vFmt, ...)
#   define  LOG_FATAL(vLogName, vFmt, ...)
#   define  LOG_SHUTDOWN_ALL()
#endif

//初始化一种类型的日志：如果该类型日志已存在，
//则重新初始化，如果不存在，则创建。
int InitLog(const char*	 	vLogName,			/*日志类型的名称(关键字,由此定位到日志文件)*/
            const char*		vLogDir,			/*文件名称(路径)*/
            int32_t	 	    vPriority = kLevelNotset,	/*日志等级*/
            uint32_t        vMaxFileSize = 10*1024*1024,	/*回卷文件最大长度*/
            uint32_t        vMaxBackupIndex = 1,		/*回卷文件个数*/
            bool            vAppend = true);                /*是否截断(默认即可)*/

//重新给已存在的日志赋值，
//但是不能改变日志的名称，以及定位的文件。
int ReInitLog(const char* 	vLogName, 
            int32_t         vPriority = kLevelNotset,	/*日志等级*/
            uint32_t        vMaxFileSize = 10*1024*1024,	/*回卷文件最大长度*/
            uint32_t        vMaxBackupIndex = 1);		/*回卷文件个数*/

//关闭所有类新的日志，
//(包括文件句柄和清除相关对象),在程序退出前用.
int ShutdownAllLog();

//具体日志记录函数，写入创建时关联的文件.
int LogDebug(const char* vLogName, const char* vFmt, ... );
int LogInfo	(const char* vLogName, const char* vFmt, ... );
int LogNotice(const char* vLogName, const char* vFmt, ... );
int LogWarn(const char* vLogName, const char* vFmt, ... );
int LogError(const char* vLogName, const char* vFmt, ... );
int LogFatal(const char* vLogName, const char* vFmt, ... );
int Log(const char* vLogName, int vPriority, const char* vFmt, ... );

void Log_bin(const char* vLogName, void* vBin, int vBinLen );

int LogDebug_va(const char* vLogName, const char* vFmt, va_list ap);
int LogNotice_va(const char* vLogName, const char* vFmt, va_list ap);
int LogInfo_va(const char* vLogName, const char* vFmt, va_list ap );
int LogWarn_va(const char* vLogName, const char* vFmt, va_list ap );
int LogError_va(const char* vLogName, const char* vFmt, va_list ap );
int LogFatal_va(const char* vLogName, const char* vFmt, va_list ap );
int Log_va(const char* vLogName, int vPriority, const char* vFmt, va_list ap );

}   // namespace hnet

#endif
