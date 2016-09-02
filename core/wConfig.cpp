
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

int wConfig::GetOption(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char* p = (char *) argv[i];
        if (*p++ != '-') {
            LOG_ERROR(kErrLogKey, "[system] invalid option: \"%s\"", argv[i]);
            return -1;
        }

        while (*p) {
            switch (*p++) {
            case '?':
            case 'v':
                mShowVer = 1;
                break;

            case 'd':
                mDaemon = 1;
                break;

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
                return -1;

            case 'p':
                if (*p) {
                    mPort = atoi(p);
                    goto next;
                }

                if (argv[++i]) {
                    mPort = atoi(argv[i]);
                    goto next;
                }

                LOG_ERROR(kErrLogKey, "[system] option \"-h\" requires port number");
                return -1;

            case 's':
                if (*p) {
                    mSignal = (char *) p;
                    goto next;
                }

                if (argv[++i]) {
                    mSignal = (char *) argv[i];
                    goto next;
                }

                LOG_ERROR(kErrLogKey, "[system] option \"-h\" requires signal number");
                return -1;	
            default:
                LOG_ERROR(kErrLogKey, "[system] invalid option: \"%c\"", *(p - 1));
                return -1;
            }
        }
    next:
        continue;
    }

    InitProcTitle(argc, argv);
    return 0;
}

void wConfig::InitProcTitle(int argc, const char *argv[]) {
    mProcTitle = new wProcTitle(argc, argv);
    mProcTitle->InitSetproctitle();
}

}   // namespace hnet
