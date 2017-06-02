
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wSlice.h"
#include "wLogger.h"

namespace hnet {

wConfig::wConfig() : mPool(NULL), mProcTitle(NULL) {
	error::StrerrorInit(); // 初始化错误列表
    http::StatusCodeInit(); // 初始化http状态码
	SAFE_NEW(wMemPool, mPool); // 初始化内存池
}

wConfig::~wConfig() {
    SAFE_DELETE(mPool);
    SAFE_DELETE(mProcTitle);
}

// 参数形式
// ./bin/server -?
// ./bin/server -d
// ./bin/server -h127.0.0.1  
// ./bin/server -h 127.0.0.1
int wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        const char* p = argv[i];
        if (*p++ != '-') {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "should \"-\" begin");
            return -1;
        }

        while (*p) {
            switch (*p++) {
            case '?':
            	SetBoolConf("help", true);
            	break;
            case 'v':
            	SetBoolConf("version", true);
            	break;
            case 'd':
            	SetBoolConf("daemon", true);
            	break;

            case 's':
                if (*p) {
                	SetStrConf("signal", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	SetStrConf("signal", p);
                    goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-s\" requires signal");
                return -1;

            case 'h':
                if (*p) {
                	SetStrConf("host", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	SetStrConf("host", p);
                    goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-h\" requires host");
                return -1;

            case 'p':
                if (*p) {
                	int i = atoi(p);
                	SetIntConf("port", i);
                	goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	int i = atoi(p);
                	SetIntConf("port", i);
                	goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-p\" requires port");
                return -1;

            case 'x':
                if (*p) {
                    SetStrConf("protocol", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                    SetStrConf("protocol", p);
                    goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-x\" requires protocol");
                return -1;

            case 'P':
                if (*p) {
                	SetStrConf("pid_path", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	SetStrConf("pid_path", p);
                    goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-P\" requires pid path");
                return -1;

            case 'l':
                if (*p) {
                	SetStrConf("log_path", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	SetStrConf("log_path", p);
                    goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-l\" requires log path");
                return -1;

            case 'n':
                if (*p) {
                	int i = atoi(p);
                	SetIntConf("worker", i);
                	goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	int i = atoi(p);
                	SetIntConf("worker", i);
                	goto next;
                }
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-n\" requires workers num");
                return -1;

            default:
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "");
                return -1;
            }
        }
    next:
        continue;
    }
	SAFE_NEW(wProcTitle, mProcTitle);
    
    return InitProcTitle(argc, argv);
}

bool wConfig::SetBoolConf(const std::string& key, bool val, bool force) {
	if (!force && mConf.find(key) != mConf.end()) {
		return false;
	}
	bool* b = reinterpret_cast<bool *>(mPool->Allocate(sizeof(bool)));
	*b = val;
	mConf[key] = reinterpret_cast<void *>(b);
	return true;
}

bool wConfig::SetIntConf(const std::string& key, int val, bool force) {
	if (!force && mConf.find(key) != mConf.end()) {
		return false;
	}
	int* i = reinterpret_cast<int *>(mPool->Allocate(sizeof(int)));
	*i = val;
	mConf[key] = reinterpret_cast<void *>(i);
	return true;
}

bool wConfig::SetStrConf(const std::string& key, const char *val, bool force) {
	if (!force && mConf.find(key) != mConf.end()) {
		return false;
	}
	char* c = mPool->Allocate(sizeof(std::string));
	std::string *s;
	// 初始化string对象（string使用前必须初始化）
	SAFE_NEW((c)std::string(), s);
	*s = val;
	mConf[key] = reinterpret_cast<void *>(s);
	return true;
}

}   // namespace hnet
