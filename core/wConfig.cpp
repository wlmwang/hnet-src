
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wSlice.h"

namespace hnet {

wConfig::wConfig() : mPool(NULL), mProcTitle(NULL) {
	SAFE_NEW(wMemPool, mPool);
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
const wStatus& wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        const char* p = argv[i];
        if (*p++ != '-') {
            return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
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
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires signal");

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
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-h\" requires host address");

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
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");

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
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-P\" requires pid path");

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
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-l\" requires log path address");

            default:
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
            }
        }
    next:
        continue;
    }

	SAFE_NEW(wProcTitle, mProcTitle);
    return mStatus = InitProcTitle(argc, argv);
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
	// 定位new操作符，初始化string对象（string使用前必须初始化）
	SAFE_NEW((c)std::string(), s);
	*s = val;
	mConf[key] = reinterpret_cast<void *>(s);
	return true;
}

}   // namespace hnet
