
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wSlice.h"
#include "tinyxml.h"    //lib tinyxml
#include "wProcTitle.h"

namespace hnet {

wConfig::~wConfig() {
    SAFE_DELETE(mProcTitle);
}

// 参数形式 
// ./bin/server -?
// ./bin/server -d
// ./bin/server -h127.0.0.1  
// ./bin/server -h 127.0.0.1
wStatus wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char* p = argv[i];
        if (*p++ != '-') {
            return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
        }

        while (*p) {
            switch (*p++) {
            case '?':
                mShowHelp = true;  break;
            case 'v':
                mShowVer = true;   break;
            case 'd':
                mDaemon = true;    break;

            // 监听ip
            case 'h':
                if (*p) {
                    mHost = p;
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                    mHost = p;
                    goto next;
                }
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-h\" requires ip address");

            case 'p':
                uint64_t val;
                if (*p) {
                    if (misc::ConsumeDecimalNumber(&wSlice(p, strlen(p)), &val)) {
                        mPort = static_cast<uint16_t>(val);
                        goto next;
                    }
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                    if (misc::ConsumeDecimalNumber(&wSlice(p, strlen(p)), &val)) {
                        mPort = static_cast<uint16_t>(val);
                        goto next;
                    }
                }
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");

            case 's':
                if (*p) {
                    mSignal = p;
                    goto next;
                }

                p = argv[++i]; // 多一个空格
                if (*p) {
                    mSignal = p;
                    goto next;
                }
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires signal");
            
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
    mProcTitle = new wProcTitle(argc, argv);
    return mStatus = mProcTitle->InitSetproctitle();
}

}   // namespace hnet
