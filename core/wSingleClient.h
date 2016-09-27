
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SINGLE_CLIENT_H_
#define _W_SINGLE_CLIENT_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wTask;

// 单连接、同步接口(使用task同步接口)客户端
class wSingleClient : private wNoncopyable {
public:
    wSingleClient();
    ~wSingleClient();

    wStatus Connect(std::string ipaddr, uint16_t port, std::string protocol = "TCP");
    wTask* Task() { return mTask;}
    
protected:
    wStatus mStatus;
    wTask* mTask;
};

}	// namespace hnet

#endif