
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
    virtual const wStatus& GetOption(int argc, const char *argv[]);

    inline const wStatus& Setproctitle(const char* pretitle, const char* title) {
    	return mStatus = mProcTitle->Setproctitle(pretitle, title);
    }

    inline const wStatus& InitProcTitle(int argc, const char *argv[]) {
        if (mProcTitle == NULL) {
            return mStatus = wStatus::IOError("wConfig::InitProcTitle", "new failed");
        }
        return mStatus = mProcTitle->SaveArgv(argc, argv);
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

    // 设置配置函数
    bool SetBoolConf(const std::string& key, bool val, bool force = true);
    bool SetIntConf(const std::string& key, int val, bool force = true);
    bool SetStrConf(const std::string& key, const char *val, bool force = true);

protected:
    std::map<std::string, void*> mConf;
    wMemPool *mPool;
    wProcTitle *mProcTitle;
    wStatus mStatus;
};

}    // namespace hnet

#endif
