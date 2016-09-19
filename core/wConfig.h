
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CONFIG_H_
#define _W_CONFIG_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wProcTitle;

class wConfig : private wNoncopyable {
public:
    wConfig() : mShowVer(false), mShowHelp(false), mDaemon(false), mSignal(NULL), mHost(NULL), mPort(0), mProcTitle(NULL) { }
    virtual ~wConfig();
    virtual wStatus GetOption(int argc, const char *argv[]);

public:
    bool mShowVer;
    bool mShowHelp;
    bool mDaemon;
    char *mSignal;
    char *mHost;
    uint16_t mPort;
    
    wProcTitle *mProcTitle;
    
protected:
    wStatus mStatus;

    wStatus InitProcTitle(int argc, const char *argv[]);
};

}    // namespace hnet

#endif