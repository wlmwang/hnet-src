
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CLIENT_H_
#define _W_CLIENT_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wTask;

// 单客户端类
// 请使用task同步消息接口（发送、接受）
class wClient : private wNoncopyable {
public:
	wClient(int32_t type);
	~wClient();
    
	wStatus Connect(std::string ipaddr, uint16_t port, std::string protocol = "TCP");
	wTask* Task() { return mTask;}

protected:
	wStatus mStatus;
	int32_t mType;
	wTask* mTask;
};

}	// namespace hnet

#endif