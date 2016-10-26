
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wLogger.h"
#include "wMisc.h"
#include "wMutex.h"

namespace hnet {

void wPosixLogger::Logv(const char* format, va_list ap) {
	mMutex.Lock();
	const uint64_t thread_id = (*mGettid)();

	// 尝试两种缓冲方式 字符串整理
	char buffer[500];
	for (int iter = 0; iter < 2; iter++) {
		char* base;   // 缓冲地址
		int bufsize;  // 缓冲长度
		if (iter == 0) {
			bufsize = sizeof(buffer);
			base = buffer;
		} else {
			bufsize = 30000;
			SAFE_NEW_VEC(bufsize, char, base);
		}
		char* p = base;
		char* limit = base + bufsize;

		struct timeval tv;
		gettimeofday(&tv, NULL);
		const time_t seconds = tv.tv_sec;
		struct tm t;
		localtime_r(&seconds, &t);
		p += snprintf(p, limit - p, "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
				t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
				static_cast<int>(tv.tv_usec), static_cast<long long unsigned int>(thread_id));

		if (p < limit) {
			va_list backup_ap;
			va_copy(backup_ap, ap);
			p += vsnprintf(p, limit - p, format, backup_ap);
			va_end(backup_ap);
		}

		if (p >= limit) {
			if (iter == 0) {
				continue;
			} else {
				p = limit - 1;
			}
		}

		// 结尾非\n
		if (p == base || p[-1] != '\n') {
			*p++ = '\n';
		}

		assert(p <= limit);
		fwrite(base, 1, p - base, mFile);
		fflush(mFile);

		// 如果是new申请的缓冲地址，则它不会等于buffer的栈地址
		if (base != buffer) {
			SAFE_DELETE_VEC(base);
		}
		break;
	}
	mMutex.Unlock();
}

// 写日志函数接口
void Log(wLogger* log, const char* format, ...) {
	if (log != NULL) {
		va_list ap;
		va_start(ap, format);
		log->Logv(format, ap);
		va_end(ap);
	}
}

}	// namespace hnet
