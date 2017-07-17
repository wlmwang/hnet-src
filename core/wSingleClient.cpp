
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include "wSingleClient.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"
#include "wHttpTask.h"

namespace hnet {

wSingleClient::~wSingleClient() {
    HNET_DELETE(mTask);
}

int wSingleClient::Connect(const std::string& ipaddr, uint16_t port, const std::string& protocol) {
    wSocket *socket = NULL;
    if (protocol == "TCP") {
       HNET_NEW(wTcpSocket(kStConnect), socket);
    } else if (protocol == "HTTP") {
        HNET_NEW(wTcpSocket(kStConnect, kSpHttp), socket);
    } else if (protocol == "UNIX") {
       HNET_NEW(wUnixSocket(kStConnect), socket);
    }

    if (!socket) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::Connect new() failed", error::Strerror(errno).c_str());
        return -1;
    }
	
    int ret = socket->Open();
    if (ret == -1) {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::Connect Open() failed", "");
        return ret;
    }

    ret = socket->Connect(ipaddr, port);
    if (ret == -1) {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::Connect Connect() failed", "");
        return ret;
    }

    socket->SS() = kSsConnected;

    HNET_DELETE(mTask);
    if (protocol == "TCP") {
        HNET_NEW(wTcpTask(socket), mTask);
    } else if (protocol == "HTTP") {
        HNET_NEW(wHttpTask(socket), mTask);
    } else if (protocol == "UNIX") {
        HNET_NEW(wUnixTask(socket), mTask);
    } else {
        HNET_DELETE(socket);
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::Connect () failed", "unknown sp");
        return -1;
    }

    if (!mTask) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::Connect new() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

int wSingleClient::HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout) {
    int ret = mTask->HttpGet(url, header, res, timeout);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::HttpGet HttpGet() failed", "");
    }
    return ret;
}

int wSingleClient::HttpPost(const std::string& url, const std::map<std::string, std::string>& data, 
    const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout) {
    int ret = mTask->HttpPost(url, data, header, res, timeout);
    if (ret == -1) {
        HNET_ERROR(soft::GetLogPath(), "%s : %s", "wSingleClient::HttpPost HttpPost() failed", "");
    }
    return ret;
}

}   // namespace hnet
