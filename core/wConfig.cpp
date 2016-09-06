
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wSlice.h"
#include "wLog.h"
#include "wProcTitle.h"
#include "tinyxml.h"    //lib tinyxml

namespace hnet {

wConfig::~wConfig() {
    misc::SafeDelete(mProcTitle);
}

// 参数形式 ./bin/server -? -d  -h127.0.0.1  -h 127.0.0.1
wStatus wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char* p = (char *) argv[i];
        if (*p++ != '-') {
            mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
            LOG_ERROR(kErrLogKey, mStatus.ToString());
            return mStatus;
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

                p = (char *) argv[++i]; // 多一个空格
                if (*p) {
                    mHost = p;
                    goto next;
                }
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-h\" requires ip address");
                LOG_ERROR(kErrLogKey, mStatus.ToString());
                return mStatus;

            case 'p':
                uint64_t val;
                if (*p) {
                    if (misc::ConsumeDecimalNumber(&wSlice(p, strlen(p)), &val)) {
                        mPort = static_cast<uint16_t>(val);
                        goto next;
                    }
                }

                p = (char *) argv[++i]; // 多一个空格
                if (*p) {
                    if (misc::ConsumeDecimalNumber(&wSlice(p, strlen(p)), &val)) {
                        mPort = static_cast<uint16_t>(val);
                        goto next;
                    }
                }
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");
                LOG_ERROR(kErrLogKey, mStatus.ToString());
                return mStatus;

            case 's':
                if (*p) {
                    mSignal = p;
                    goto next;
                }

                p = (char *) argv[++i]; // 多一个空格
                if (*p) {
                    mSignal = p;
                    goto next;
                }
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires signal");
                LOG_ERROR(kErrLogKey, mStatus.ToString());
                return mStatus;
            
            default:
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
                LOG_ERROR(kErrLogKey, mStatus.ToString());
                return mStatus;
            }
        }
    next:
        continue;
    }

    return mStatus = InitProcTitle(argc, argv);
}

wStatus wConfig::InitProcTitle(int argc, const char *argv[]) {
    mProcTitle = new wProcTitle(argc, argv);
    if (mProcTitle->InitSetproctitle() != 0) {
        return wStatus::Corruption("wConfig::InitProcTitle", "Unkown error");
    }
    return wStatus::Nothing();
}

}   // namespace hnet
