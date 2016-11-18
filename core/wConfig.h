
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CONFIG_H_
#define _W_CONFIG_H_

#include <map>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wProcTitle.h"
#include "wMemPool.h"

namespace hnet {

class wConfig : private wNoncopyable {
public:
    wConfig();
    virtual ~wConfig();
    virtual wStatus GetOption(int argc, const char *argv[]);

    inline wStatus Setproctitle(const char* pretitle, const char* title) {
    	return mProcTitle->Setproctitle(pretitle, title);
    }

    inline wStatus InitProcTitle(int argc, const char *argv[]) {
        if (mProcTitle == NULL) {
            return wStatus::IOError("wConfig::InitProcTitle", "new failed");
        }
        return mProcTitle->SaveArgv(argc, argv);
    }

    template<typename T>
    bool GetConf(const std::string& key, T* val) {
    	std::map<std::string, void*>::iterator it = mConf.find(key);
    	if (it != mConf.end()) {
    		*val = *(reinterpret_cast<T*>(it->second));
    		return true;
    	}
    	return false;
    }

protected:
    wStatus mStatus;
    std::map<std::string, void*> mConf;
    wMemPool *mPool;
    wProcTitle *mProcTitle;
};

}    // namespace hnet

#endif
