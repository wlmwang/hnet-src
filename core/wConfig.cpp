
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wLogger.h"
#include "wMemPool.h"
#include "wProcTitle.h"

namespace hnet {

wConfig::wConfig(): mPool(NULL), mProcTitle(NULL) {
    soft::TimeUpdate(); // 时间初始化
	error::StrerrorInit(); // 初始化错误列表
    http::StatusCodeInit(); // 初始化http状态码

	HNET_NEW(wMemPool, mPool);
    HNET_NEW(wProcTitle, mProcTitle);
}

wConfig::~wConfig() {
    HNET_DELETE(mPool);
    HNET_DELETE(mProcTitle);
}

// 参数形式
// ./bin/server -?
// ./bin/server -d
// ./bin/server -h127.0.0.1  
// ./bin/server -h 127.0.0.1
int wConfig::GetOption(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        const char* p = argv[i];
        if (*p++ != '-') {
            HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "should \"-\" begin");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-s\" requires signal");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-h\" requires host");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-p\" requires port");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-x\" requires protocol");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-P\" requires pid path");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-l\" requires log path");
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
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "option \"-n\" requires workers num");
                return -1;

            default:
                HNET_ERROR(soft::GetLogPath(), "%s : %s", "wConfig::GetOption failed, invalid option", "");
                return -1;
            }
        }
    next:
        continue;
    }
    return InitProcTitle(argc, argv);
}

int wConfig::InitProcTitle(int argc, char *argv[]) {
    return mProcTitle->SaveArgv(argc, argv);
}

int wConfig::Setproctitle(const char* pretitle, const char* title, bool attach) {
    return mProcTitle->Setproctitle(pretitle, title, attach);
}

char** wConfig::Argv() {
    return mProcTitle->Argv();
}

char** wConfig::Environ() {
    return mProcTitle->Environ();
}

bool wConfig::SetBoolConf(const std::string& key, bool val, bool force) {
	if (!force && mConf.find(key) != mConf.end()) {
		return false;
	}
	bool* b = reinterpret_cast<bool*>(mPool->Allocate(sizeof(bool)));
	*b = val;
	mConf[key] = reinterpret_cast<void*>(b);
	return true;
}

bool wConfig::SetIntConf(const std::string& key, int val, bool force) {
	if (!force && mConf.find(key) != mConf.end()) {
		return false;
	}
	int* i = reinterpret_cast<int*>(mPool->Allocate(sizeof(int)));
	*i = val;
	mConf[key] = reinterpret_cast<void*>(i);
	return true;
}

bool wConfig::SetStrConf(const std::string& key, const char *val, bool force) {
	if (!force && mConf.find(key) != mConf.end()) {
		return false;
	}
	char* c = mPool->Allocate(sizeof(std::string));
	
    // 初始化string对象（string使用前必须初始化）
    std::string *s;  
	HNET_NEW((c)std::string(), s);
	*s = val;
	mConf[key] = reinterpret_cast<void*>(s);
	return true;
}

}   // namespace hnet
