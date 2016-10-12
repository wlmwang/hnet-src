
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wSlice.h"
#include "tinyxml.h"    //lib tinyxml

namespace hnet {

wConfig::~wConfig() {
	for (std::map<std::string, void*>::iterator it = mConf.begin(); it != mConf.end; it++) {
		SAFE_DELETE_VEC(it->second);
	}
    SAFE_DELETE(mProcTitle);
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
        	bool* b;
        	SAFE_NEW_VEC(1, bool, b);
        	*b = val;
        	this->mConf[key] = reinterpret_cast<void *>(b);
        };

        // 设置整型值
        auto setIntConf = [this](std::string key, int val) {
        	int* i;
        	SAFE_NEW_VEC(1, bool, i);
        	*b = val;
        	this->mConf[key] = reinterpret_cast<void *>(i);
        };

        // 设置std::string值
        auto setStrConf = [this](std::string key, const char *val) {
        	std::string* s;
        	SAFE_NEW_VEC(1, std::string, s);
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
                	int p = atoi(p);
                	setIntConf("port", p);
                	goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                	int p = atoi(p);
                	setIntConf("port", p);
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

wStatus wConfig::InitProcTitle(int argc, const char *argv[]) {
    SAFE_NEW(wProcTitle(argc, argv), mProcTitle);
    if (mProcTitle == NULL) {
        return wStatus::IOError("wConfig::InitProcTitle", "new failed");
    }
    return mProcTitle->InitSetproctitle();
}

}   // namespace hnet
