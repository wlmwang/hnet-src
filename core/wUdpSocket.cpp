
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include "wUdpSocket.h"
#include "wMisc.h"

namespace hnet {

const wStatus& wUdpSocket::Open() {
	if ((mFD = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		return mStatus = wStatus::AccessIllegal("wUdpSocket::Open socket() AF_INET failed", error::Strerror(errno));
	}
	return mStatus.Clear();
}

const wStatus& wUdpSocket::Bind(const std::string& host, uint16_t port) {
	struct sockaddr_in socketAddr;
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons(static_cast<short int>(port));
	socketAddr.sin_addr.s_addr = inet_addr(host.c_str());
	if (bind(mFD, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr)) == -1) {
		return mStatus = wStatus::IOError("wUdpSocket::Bind bind failed", error::Strerror(errno));
	}
	return mStatus.Clear();
}

const wStatus& wUdpSocket::Listen(const std::string& host, uint16_t port) {
	mHost = host;
	mPort = port;
	if (!Bind(mHost, port).Ok()) {
		return mStatus;
	}
	return SetFL();
}

const wStatus& wUdpSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
	mRecvTm = misc::GetTimeofday();
    if (mClientHost.size() == 0 || mClientPort == 0) {
        struct sockaddr_in socketAddr;
        socklen_t addrLen;
        *size = recvfrom(mFD, reinterpret_cast<void*>(buf), len, 0, reinterpret_cast<struct sockaddr *>(&socketAddr), &addrLen);
        if (*size == 0) {
            mStatus = wStatus::IOError("wUdpSocket::RecvBytes, client was closed", "");
        } else if (*size == -1 && (errno == EINTR || errno == EAGAIN)) {
            mStatus.Clear();
        } else if (*size == -1) {
            mStatus = wStatus::IOError("wUdpSocket::RecvBytes, recvfrom failed", error::Strerror(errno));
        }
        mClientHost = inet_ntoa(socketAddr.sin_addr);
        mClientPort = socketAddr.sin_port;
    }
    return mStatus;
}

const wStatus& wUdpSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
	mSendTm = misc::GetTimeofday();
	if (mClientHost.size() != 0 && mClientPort != 0) {
		struct sockaddr_in socketAddr;
		socketAddr.sin_family = AF_INET;
		socketAddr.sin_port = htons((short)mClientPort);
		socketAddr.sin_addr.s_addr = inet_addr(mClientHost.c_str());
		*size = sendto(mFD, reinterpret_cast<void*>(buf), len, 0, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr));
	    if (*size >= 0) {
	        mStatus.Clear();
	    } else if (*size == -1 && (errno == EINTR || errno == EAGAIN)) {
	        mStatus.Clear();
	    } else {
	        mStatus = wStatus::IOError("wUdpSocket::SendBytes, sendto failed", error::Strerror(errno));
	    }
	    mClientHost = "";
	    mClientPort = 0;
	}
    return mStatus;
}

}	// namespace hnet
