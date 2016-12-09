
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSingleClient.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"

namespace hnet {

wSingleClient::wSingleClient() : mTask (NULL) { }

wSingleClient::~wSingleClient() {
    SAFE_DELETE(mTask);
}

const wStatus& wSingleClient::Connect(const std::string& ipaddr, uint16_t port, std::string protocol) {
    wSocket *socket;
    if (protocol == "TCP") {
	   SAFE_NEW(wTcpSocket(kStConnect), socket);
    } else if(protocol == "UNIX") {
	   SAFE_NEW(wUnixSocket(kStConnect), socket);
    } else {
        socket = NULL;
    }
    if (socket == NULL) {
    	return mStatus = wStatus::IOError("wSingleClient::Connect", "socket new failed");
    }
	
    ;
    if (!(mStatus = socket->Open()).Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }

    int64_t ret;
    if (!(mStatus = socket->Connect(&ret, ipaddr, port)).Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }
    socket->SS() = kSsConnected;
	
    if (protocol == "TCP") {
	   SAFE_NEW(wTcpTask(socket), mTask);
    } else if(protocol == "UNIX") {
	   SAFE_NEW(wUnixTask(socket), mTask);
    }
    if (mTask == NULL) {
	   return mStatus = wStatus::IOError("wSingleClient::Connect", "task new failed");
    }
    return mStatus.Clear();
}

}   // namespace hnet
