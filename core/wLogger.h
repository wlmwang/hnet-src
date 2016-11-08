
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_LOGGER_H_
#define _W_LOGGER_H_

#include <map>
#include <cstdarg>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

#ifdef _USE_LOGGER_
#	define  LOG_ERROR(logpath, fmt, ...)	hnet::Log(logpath, fmt, ##__VA_ARGS__)
#	ifdef _DEBUG_
#		define  LOG_DEBUG(logpath, fmt, ...)
#	else
#		define  LOG_DEBUG(logpath, fmt, ...)	hnet::Log(logpath, fmt, ##__VA_ARGS__)
#	endif
#else
#	define  LOG_ERROR(logpath, fmt, ...)
#	define  LOG_DEBUG(logpath, fmt, ...)
#endif

namespace hnet {

const uint8_t kLogNum = 64;

// 日志接口
class wLogger : private wNoncopyable {
public:
	wLogger() { }
    virtual ~wLogger() { }

    // 写一条指定格式的日志到文件中
    virtual void Logv(const char* format, va_list ap) = 0;
};

// 实现类
// 日志实现类
class wPosixLogger : public wLogger {
public:
	wPosixLogger(const std::string& fname, uint64_t (*gettid)(), off_t maxsize = 32*1024*1024) : mFname(fname), mGettid(gettid), mMaxsize(maxsize) {
		mFile = OpenCreatLog(mFname, "a+");
	}

	virtual ~wPosixLogger() {
		CloseLog(mFile);
	}

	// 写日志。最大3000字节，多的截断
	virtual void Logv(const char* format, va_list ap);

private:
	void ArchiveLog();

	inline FILE* OpenCreatLog(const std::string& fname, const std::string& mode) {
		return fopen(fname.c_str(), mode.c_str());
	}

	inline void CloseLog(FILE* f) {
		fclose(f);
	}

	std::string mFname;
	uint64_t (*mGettid)();	// 获取当前线程id函数指针
	off_t mMaxsize;
	FILE* mFile;
};

// 写日志函数接口
extern void Log(const std::string& logpath, const char* format, ...);

}	// namespace hnet

#endif
