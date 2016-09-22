
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wClient.h"
#include "wTcpSocket.h"
#include "wUnixSocket.h"
#include "wTcpTask.h"
#include "wUnixTask.h"

wClient::wClient(int32_t type) : mType(type) { }

wClient::~wClient() {
	SAFE_DELETE(mTask);
}

wStatus wClient::Connect(std::string ipaddr, uint16_t port, std::string protocol) {
    wSocket *socket = NULL;
    if (protocol == "TCP") {
		SAFE_NEW(wTcpSocket(kStConnect), socket);
    } else if(protocol == "UNIX") {
		SAFE_NEW(wUnixSocket(kStConnect), socket);
    }
    
    if (socket == NULL) {
    	return mStatus = wStatus::IOError("wServer::AddListener", "new failed");
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
		return mStatus = wStatus::IOError("wClient::Connect", "new failed");
    }
    return mStatus;
}
