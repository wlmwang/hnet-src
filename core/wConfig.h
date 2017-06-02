
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CONFIG_H_
#define _W_CONFIG_H_

#include <map>
#include "wCore.h"
#include "wNoncopyable.h"
#include "wProcTitle.h"
#include "wMemPool.h"

namespace hnet {

class wConfig : private wNoncopyable {
public:
    wConfig();
    virtual ~wConfig();
    virtual int GetOption(int argc, const char *argv[]);

    inline int Setproctitle(const char* pretitle, const char* title, bool attach = true) {
        return mProcTitle->Setproctitle(pretitle, title, attach);
    }
    
    inline int InitProcTitle(int argc, const char *argv[]) {
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

    // 设置配置函数
    bool SetBoolConf(const std::string& key, bool val, bool force = true);
    bool SetIntConf(const std::string& key, int val, bool force = true);
    bool SetStrConf(const std::string& key, const char *val, bool force = true);

protected:
    std::map<std::string, void*> mConf;
    wMemPool *mPool;
    wProcTitle *mProcTitle;
};

}    // namespace hnet

#endif
