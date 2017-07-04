
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SINGLE_CLIENT_H_
#define _W_SINGLE_CLIENT_H_

#include <map>
#include "wCore.h"
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

    int Connect(const std::string& ipaddr, uint16_t port, const std::string& protocol = "TCP");

    inline int SyncSend(char cmd[], size_t len, ssize_t *size) {
    	return mTask->SyncSend(cmd, len, size);
    }
#ifdef _USE_PROTOBUF_
    inline int SyncSend(const google::protobuf::Message* msg, ssize_t *size) {
    	return mTask->SyncSend(msg, size);
    }
#endif

    inline int SyncRecv(char cmd[], ssize_t *size, size_t msglen = 0, uint32_t timeout = 30) {
    	return mTask->SyncRecv(cmd, size, msglen, timeout);
    }
#ifdef _USE_PROTOBUF_
    inline int SyncRecv(google::protobuf::Message* msg, ssize_t *size, size_t msglen = 0, uint32_t timeout = 30) {
    	return mTask->SyncRecv(msg, size, msglen, timeout);
    }
#endif

    int HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);
    int HttpPost(const std::string& url, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);

protected:
    wTask* mTask;
};

}	// namespace hnet

#endif
