
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

    wStatus SyncSend(char cmd[], size_t len, ssize_t *size) {
    	return mStatus = mTask->SyncSend(cmd, len, size);
    }

    wStatus SyncRecv(char cmd[], size_t len, ssize_t *size, uint32_t timeout = 30) {
    	return mStatus = mTask->SyncRecv(cmd, len, size, timeout);
    }
protected:
    wStatus mStatus;
    wTask* mTask;
};

}	// namespace hnet

#endif
