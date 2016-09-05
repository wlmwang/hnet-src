
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wMisc.h"
#include "wLog.h"
#include "wProcTitle.h"
#include "tinyxml.h"    //lib tinyxml

namespace hnet {

wConfig::~wConfig() {
    misc::SafeDelete(mProcTitle);
}

wStatus wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char* p = (char *) argv[i];
        if (*p++ != '-') {
            mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
            LOG_ERROR(kErrLogKey, mStatus.ToString().c_str());
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

            case 'h':
                if (*p) {
                    mHost = p;
                    goto next;
                }
                if (argv[++i]) {
                    mHost = (char *) argv[i];
                    goto next;
                }
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-h\" requires ip address");
                LOG_ERROR(kErrLogKey, mStatus.ToString().c_str());
                return mStatus;

            case 'p':
                if (*p) {
                    mPort = atoi(p);
                    goto next;
                }
                if (argv[++i]) {
                    mPort = atoi(argv[i]);
                    goto next;
                }
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");
                LOG_ERROR(kErrLogKey, mStatus.ToString().c_str());
                return mStatus;

            case 's':
                if (*p) {
                    mSignal = (char *) p;
                    goto next;
                }
                if (argv[++i]) {
                    mSignal = (char *) argv[i];
                    goto next;
                }
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires signal");
                LOG_ERROR(kErrLogKey, mStatus.ToString().c_str());
                return mStatus;
            
            default:
                mStatus = wStatus::InvalidArgument("wConfig::GetOption", "invalid option");
                LOG_ERROR(kErrLogKey, mStatus.ToString().c_str());
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
