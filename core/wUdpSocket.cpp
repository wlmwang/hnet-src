
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

int wUdpSocket::Open() {
	mFD = socket(AF_INET, SOCK_DGRAM, 0);
	if (mFD == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::Open socket() failed", error::Strerror(errno).c_str());
		return -1;
	}
	return 0;
}

int wUdpSocket::Bind(const std::string& host, uint16_t port) {
	struct sockaddr_in socketAddr;
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons(static_cast<short int>(port));
	socketAddr.sin_addr.s_addr = misc::Text2IP(host.c_str());
	int ret = bind(mFD, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr));
	if (ret == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::Bind bind() failed", error::Strerror(errno).c_str());
	}
	return ret;
}

int wUdpSocket::Listen(const std::string& host, uint16_t port) {
	mHost = host;
	mPort = port;

	if (Bind(mHost, mPort) == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::Listen Bind() failed", "");
		return -1;
	}

	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::Listen setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
		return -1;
	}

	if (SetNonblock() == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::Listen SetNonblock() failed", "");
		return -1;
	}
	return 0;
}

int wUdpSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
	mRecvTm = soft::TimeUsec();

	int ret = 0;
    if (mClientHost.size() == 0 || mClientPort == 0) {
        struct sockaddr_in socketAddr;
        socklen_t addrLen;
        *size = recvfrom(mFD, reinterpret_cast<void*>(buf), len, 0, reinterpret_cast<struct sockaddr *>(&socketAddr), &addrLen);
        if (*size == 0) {	// FIN package // client was closed
        	ret = -1;
        } else if (*size < 0) {
        	if (errno == EAGAIN || errno == EWOULDBLOCK) {	// Resource temporarily unavailable // 资源暂时不够(可能读缓冲区没有数据)
        		ret = 0;
        	} else if (errno == EINTR) {	// Interrupted system call
        		ret = 0;
        	} else {
	            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::RecvBytes recvfrom() failed", error::Strerror(errno).c_str());
	            ret = -1;
        	}
        }
        mClientHost = inet_ntoa(socketAddr.sin_addr);
        mClientPort = socketAddr.sin_port;
    }
    return ret;
}

int wUdpSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
	mSendTm = soft::TimeUsec();

	int ret = 0;
	if (mClientHost.size() != 0 && mClientPort != 0) {
		struct sockaddr_in socketAddr;
		socketAddr.sin_family = AF_INET;
		socketAddr.sin_port = htons((short)mClientPort);
		socketAddr.sin_addr.s_addr = misc::Text2IP(mClientHost.c_str());
		*size = sendto(mFD, reinterpret_cast<void*>(buf), len, 0, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr));

        if (*size < 0) {
        	if (errno == EAGAIN || errno == EWOULDBLOCK) {	// Resource temporarily unavailable // 资源暂时不够(可能读缓冲区没有数据)
        		ret = 0;
        	} else if (errno == EINTR) {	// Interrupted system call
        		ret = 0;
        	} else {
	            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::SendBytes sendto() failed", error::Strerror(errno).c_str());
	            ret = -1;
        	}
        } else if (*size == 0) {
            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUdpSocket::SendBytes sendto() failed", error::Strerror(errno).c_str());
            ret = -1;
        }
	    mClientHost = "";
	    mClientPort = 0;
	}
    return ret;
}

}	// namespace hnet
