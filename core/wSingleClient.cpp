
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSingleClient.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"

wSingleClient::wSingleClient() { }

wSingleClient::~wSingleClient() {
    SAFE_DELETE(mTask);
}

wStatus wSingleClient::Connect(std::string ipaddr, uint16_t port, std::string protocol) {
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
	
    mStatus = socket->Open();
    if (!mStatus.Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }

    int64_t ret;
    mStatus = socket->Connect(&ret, ipaddr, port);
    if (!mStatus.Ok()) {
        SAFE_DELETE(socket);
        return mStatus;
    }
    socket->SS() = kSsConnected;
	
    if (protocol == "TCP") {
	   SAFE_NEW(wTcpTask(sock), mTask);
    } else if(protocol == "UNIX") {
	   SAFE_NEW(wUnixTask(sock), mTask);
    }
    if (mTask == NULL) {
	   return mStatus = wStatus::IOError("wSingleClient::Connect", "task new failed");
    }
    return mStatus;
}
