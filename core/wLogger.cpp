
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wStatus.h"
#include "wLogger.h"
#include "wEnv.h"
#include "wMisc.h"
#include "wMutex.h"

namespace hnet {

void wPosixLogger::ArchiveLog() {
	wEnv* env = wEnv::Default();
	uint64_t filesize = 0;
	if (env->GetFileSize(mFname, &filesize).Ok() && filesize >= static_cast<uint64_t>(mMaxsize)) {
		// 日志大小操作限制
		CloseLog(mFile);
		std::string realpath, dir;
		if (env->GetRealPath(mFname, &realpath).Ok()) {
			// 获取目录名
			const std::string::size_type pos = realpath.find_last_of("/");
			if (pos != std::string::npos) {
				dir = realpath.substr(0, pos);
			} else {
				dir = realpath;
			}
			// 文件列表
			std::vector<std::string> children;
			if (env->GetChildren(dir, &children).Ok() && children.size() > 0) {
				// 获取日志最大id
				uint64_t maxid = 0;
				for (std::vector<std::string>::iterator it = children.begin(); it != children.end(); it++) {
					const std::string::size_type namepos = (*it).find(mFname);
					if (namepos != std::string::npos && namepos == 0) {
						// 所有日志文件后缀
						uint64_t num;
						std::string subfix = (*it).substr((*it).find_last_of(".") + 1);
						if (subfix.size() > 0 && logging::DecimalStringToNumber(subfix, &num)) {
							maxid = maxid >= num? maxid : num;
						}
					}
				}
				// 更名日志文件
				std::string src = mFname + ".", target = mFname + ".";
				if (maxid >= kLoggerNum) {
					logging::AppendNumberTo(&src, maxid);
					env->FileExists(src) && env->DeleteFile(src).Ok();
					maxid = kLoggerNum - 1;
				}
				for (uint64_t id = maxid; id >= 1; id--) {
					src = mFname + ".", target = mFname + ".";
					logging::AppendNumberTo(&src, id);
					logging::AppendNumberTo(&target, id + 1);
					if (env->FileExists(src)) {
						env->FileExists(target) && env->DeleteFile(target).Ok();
						env->FileExists(src) && env->RenameFile(src, target).Ok();
					}
				}
			}
		}
		std::string target = mFname + ".1";
		env->FileExists(target) && env->DeleteFile(target).Ok();
		env->FileExists(mFname) && env->RenameFile(mFname, target).Ok();
		mFile = OpenCreatLog(mFname, "a+");
	}
}

void wPosixLogger::Logv(const char* format, va_list ap) {
	// 检测文件大小，归档日志
	ArchiveLog();
	// 线程id
	const uint64_t pid_id = (*mGetpid)();
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
		p += snprintf(p, limit - p, "%04d/%02d/%02d-%02d:%02d:%02d.%06d [%lld] ",
				t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
				static_cast<int>(tv.tv_usec), static_cast<unsigned long long>(pid_id));

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
}

// 日志对象
static std::map<std::string, wLogger*> g_logger;
static wLogger* Logger(const std::string& logpath) {
	std::map<std::string, wLogger*>::iterator it = g_logger.find(logpath);
	if (it == g_logger.end()) {
		wLogger* l = NULL;
		if (wEnv::Default()->NewLogger(logpath, &l).Ok()) {
			g_logger.insert(std::pair<std::string, wLogger*>(logpath, l));
			return l;
		}
	} else {
		return it->second;
	}
	return NULL;
}

// 写日志函数接口
static wMutex g_logger_mutex;
void Log(const std::string& logpath, const char* format, ...) {
	g_logger_mutex.Lock();
	wLogger* l = Logger(logpath);
	if (l != NULL) {
		va_list ap;
		va_start(ap, format);
		l->Logv(format, ap);
		va_end(ap);
	}
	g_logger_mutex.Unlock();
}

}	// namespace hnet
