
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SINGLE_CLIENT_H_
#define _W_SINGLE_CLIENT_H_

#include <map>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wTask.h"

#ifdef _USE_PROTOBUF_
#include <google/protobuf/message.h>
#endif

namespace hnet {

// 单连接、同步接口(使用task同步接口)客户端
class wSingleClient : private wNoncopyable {
public:
    wSingleClient() : mTask(NULL) { }
    ~wSingleClient();

    const wStatus& Connect(const std::string& ipaddr, uint16_t port, std::string protocol = "TCP");

    inline const wStatus& SyncSend(char cmd[], size_t len, ssize_t *size) {
    	return mStatus = mTask->SyncSend(cmd, len, size);
    }
#ifdef _USE_PROTOBUF_
    inline const wStatus& SyncSend(const google::protobuf::Message* msg, ssize_t *size) {
    	return mStatus = mTask->SyncSend(msg, size);
    }
#endif

    inline const wStatus& SyncRecv(char cmd[], ssize_t *size, uint32_t timeout = 30) {
    	return mStatus = mTask->SyncRecv(cmd, size, timeout);
    }
#ifdef _USE_PROTOBUF_
    inline const wStatus& SyncRecv(google::protobuf::Message* msg, ssize_t *size, uint32_t timeout = 30) {
    	return mStatus = mTask->SyncRecv(msg, size, timeout);
    }
#endif

    const wStatus& HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);
    const wStatus& HttpPost(const std::string& url, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);
protected:
    wTask* mTask;
    wStatus mStatus;
};

}	// namespace hnet

#endif
