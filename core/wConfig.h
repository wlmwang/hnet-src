
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CONFIG_H_
#define _W_CONFIG_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wProcTitle.h"

namespace hnet {

class wConfig : private wNoncopyable {
public:
    wConfig() : mShowVer(false), mShowHelp(false), mDaemon(false), mSignal(NULL), 
    mPidPath(NULL), mLockPath(NULL), mHost(NULL), mPtotocol(NULL), mPort(0), mProcTitle(NULL) { }

    virtual ~wConfig();
    virtual wStatus GetOption(int argc, const char *argv[]);

    template<typename T>
    bool GetConf(std::string key, T* val) {
    	std::map<std::string, void*>::iterator it = mConf.find(key);
    	if (it != mConf.end()) {
    		*val = reinterpret_cast<T>(it->second);
    		return true;
    	}
    	return false;
    }

    std::map<std::string, void*> mConf;
    wProcTitle *mProcTitle;
protected:
    wStatus mStatus;
    wStatus InitProcTitle(int argc, const char *argv[]);
};

}    // namespace hnet

#endif
