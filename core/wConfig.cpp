
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wSlice.h"
#include "tinyxml.h"    //lib tinyxml
#include "wMemPool.h"

namespace hnet {

wConfig::wConfig() : mProcTitle(NULL) {
	SAFE_NEW(wMemPool, mPool);
}

wConfig::~wConfig() {
    SAFE_DELETE(mProcTitle);
    SAFE_DELETE(mPool);
}

// 参数形式
// ./bin/server -?
// ./bin/server -d
// ./bin/server -h127.0.0.1  
// ./bin/server -h 127.0.0.1
wStatus wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        const char* p = argv[i];
        if (*p++ != '-') {
            return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
        }

        // 设置布尔值
        auto setBoolConf = [this](std::string key, bool val) {
        	bool* b = reinterpret_cast<bool *>(mPool->Allocate(sizeof(bool)));
        	*b = val;
        	this->mConf[key] = reinterpret_cast<void *>(b);
        };

        // 设置整型值
        auto setIntConf = [this](std::string key, int val) {
        	int* i = reinterpret_cast<int *>(mPool->Allocate(sizeof(int)));
        	*i = val;
        	this->mConf[key] = reinterpret_cast<void *>(i);
        };

        // 设置string值
        auto setStrConf = [this](std::string key, const char *val) {
        	char* c = mPool->Allocate(sizeof(std::string));
        	std::string *s;
        	// 定位new操作符，初始化string对象（string使用前必须初始化）
        	SAFE_NEW((c)std::string(), s);
        	*s = val;
        	this->mConf[key] = reinterpret_cast<void *>(s);
        };

        while (*p) {
            switch (*p++) {
            case '?':
            	setBoolConf("help", true);
            	break;
            case 'v':
            	setBoolConf("version", true);
            	break;
            case 'd':
            	setBoolConf("daemon", true);
            	break;

            case 's':
                if (*p) {
                	setStrConf("signal", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	setStrConf("signal", p);
                    goto next;
                }
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires signal");

            case 'h':
                if (*p) {
                	setStrConf("host", p);
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	setStrConf("host", p);
                    goto next;
                }
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-h\" requires ip address");

            case 'p':
                if (*p) {
                	int i = atoi(p);
                	setIntConf("port", i);
                	goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	int i = atoi(p);
                	setIntConf("port", i);
                	goto next;
                }
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");

            default:
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
            }
        }
    next:
        continue;
    }

    return mStatus = InitProcTitle(argc, argv);
}

}   // namespace hnet
