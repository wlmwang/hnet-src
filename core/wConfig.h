
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CONFIG_H_
#define _W_CONFIG_H_

#include <map>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

class wProcTitle;
class wMemPool;

class wConfig : private wNoncopyable {
public:
    wConfig();
    virtual ~wConfig();
    virtual int GetOption(int argc, char *argv[]);
    
    char** Argv();
    char** Environ();
    int InitProcTitle(int argc, char *argv[]);
    int Setproctitle(const char* pretitle, const char* title, bool attach = true);

    // 设置配置函数
    bool SetBoolConf(const std::string& key, bool val, bool force = true);
    bool SetIntConf(const std::string& key, int val, bool force = true);
    bool SetStrConf(const std::string& key, const char *val, bool force = true);

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
    std::map<std::string, void*> mConf;
    wMemPool *mPool;
    wProcTitle *mProcTitle;
};

}    // namespace hnet

#endif
