
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CONFIG_H_
#define _W_CONFIG_H_

#include "wCore.h"
#include "wLog.h"
#include "wMisc.h"
#include "wNoncopyable.h"
#include "tinyxml.h"	//lib tinyxml

namespace hnet {
class wProcTitle;

class wConfig: private wNoncopyable {
public:
    wConfig() : mShowVer(0), mDaemon(0), mSignal(NULL), mHost(NULL), mPort(0),mProcTitle(NULL) { }
    virtual ~wConfig() {
        misc::SafeDelete(mProcTitle);
    }
    virtual int GetOption(int argc, const char *argv[]);
    void InitProcTitle(int argc, const char *argv[]);

public:
    int mShowVer;	//版本信息
    int mDaemon;	//是否启动为守护进程
    char *mSignal;	//信号字符串
    char *mHost;
    int mPort;

    wProcTitle *mProcTitle;		//进程标题
};

}	// namespace hnet

#endif
