
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wConfig.h"
#include "wProcTitle.h"
#include "wLog.h"
#include "wMisc.h"
#include "tinyxml.h"    //lib tinyxml

namespace hnet {
wConfig::~wConfig() {
    misc::SafeDelete(mProcTitle);
}

wStatus wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char* p = (char *) argv[i];
        if (*p++ != '-') {
            LOG_ERROR(kErrLogKey, "[system] invalid option: \"%s\"", *(p + 1));
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

            case 'h':
                if (*p) {
                    mHost = p;
                    goto next;
                }
                if (argv[++i]) {
                    mHost = (char *) argv[i];
                    goto next;
                }
                LOG_ERROR(kErrLogKey, "[system] option \"-h\" requires ip address");
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-h\" requires ip address");

            case 'p':
                if (*p) {
                    mPort = atoi(p);
                    goto next;
                }
                if (argv[++i]) {
                    mPort = atoi(argv[i]);
                    goto next;
                }
                LOG_ERROR(kErrLogKey, "[system] option \"-p\" requires port number");
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");

            case 's':
                if (*p) {
                    mSignal = (char *) p;
                    goto next;
                }
                if (argv[++i]) {
                    mSignal = (char *) argv[i];
                    goto next;
                }
                LOG_ERROR(kErrLogKey, "[system] option \"-s\" requires signal number");
                return mStatus = wStatus::InvalidArgument("wConfig::GetOption", "option \"-p\" requires port number");
            
            default:
                LOG_ERROR(kErrLogKey, "[system] invalid option: \"%c\"", *(p - 1));
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
    if (mProcTitle->InitSetproctitle() != 0) {
        return wStatus::Corruption("wConfig::InitProcTitle", "Unkown error");
    }
    return wStatus::Nothing();
}

}   // namespace hnet
