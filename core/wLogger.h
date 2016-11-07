
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_LOGGER_H_
#define _W_LOGGER_H_

#include <stdarg.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wMutex.h"

#ifdef _DEBUG_
#define  LOG_DEBUG(wLogger* logger, fmt, ...)
#else
#define  LOG_DEBUG(wLogger* logger, fmt, ...)	Log(logger, fmt, ##__VA_ARGS__)
#define  LOG_ERROR(wLogger* logger, fmt, ...)	Log(logger, fmt, ##__VA_ARGS__)
#endif

namespace hnet {

// 日志接口
class wLogger : private wNoncopyable {
public:
	wLogger() { }
    virtual ~wLogger() { }

    // 写一条指定格式的日志到文件中
    virtual void Logv(const char* format, va_list ap) = 0;
};

// 写日志函数接口
extern void Log(wLogger* log, const char* format, ...);

// 实现类
// 日志实现类
class wPosixLogger : public wLogger {
public:
	wPosixLogger(FILE* f, uint64_t (*gettid)()) : mFile(f), mGettid(gettid) { }

	virtual ~wPosixLogger() {
		fclose(mFile);
	}

	// 写日志。最大3000字节，多的截断
	virtual void Logv(const char* format, va_list ap);

private:
	wMutex mMutex;
	FILE* mFile;
	// 获取当前线程id函数指针
	uint64_t (*mGettid)();
};

}	// namespace hnet

#endif
