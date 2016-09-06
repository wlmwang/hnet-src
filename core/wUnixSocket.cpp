
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/un.h>
#include <poll.h>
#include "wUnixSocket.h"

namespace hnet {

wStatus wUnixSocket::Open() {
	if ((mFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return mStatus = wStatus::IOError("wUnixSocket::Open socket() AF_UNIX failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wUnixSocket::Bind(string host, uint16_t port) {
	struct sockaddr_un stSocketAddr;
	memset(&stSocketAddr, 0, sizeof(stSocketAddr));
	stSocketAddr.sun_family = AF_UNIX;
	strncpy(stSocketAddr.sun_path, host.c_str(), sizeof(stSocketAddr.sun_path) - 1);

	if (bind(mFD, (struct sockaddr *)&stSocketAddr, sizeof(stSocketAddr)) == -1) {
		return mStatus = wStatus::IOError("wUnixSocket::Bind bind failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wUnixSocket::Listen(string host, uint16_t port) {
	mHost = host;
	
	if (!Bind(mHost).Ok()) {
		return mStatus;
	}

	if (listen(mFD, kListenBacklog) < 0) {
		return mStatus = wStatus::IOError("wUnixSocket::Listen listen failed", strerror(errno));
	}
	
	return SetFL();
}

int wUnixSocket::Connect(string host, uint16_t port, float fTimeout) {
	mHost = host;

	string tmpsock = "unix_";
	tmpsock += Itos(getpid()) + ".sock";
	if (!Bind(mHost).Ok()) {
		return -1;
	}

	struct sockaddr_un stSockAddr;
	memset(&stSockAddr, 0, sizeof(stSockAddr));
	stSockAddr.sun_family = AF_UNIX;
	strncpy(stSockAddr.sun_path, mHost.c_str(), sizeof(stSockAddr.sun_path) - 1);

	//超时设置
	if (fTimeout > 0) {
		SetFL();
		if (SetFL().Ok()) {	// todo
			return -1;
		}
	}

	int ret = connect(mFD, (const struct sockaddr *)&stSockAddr, sizeof(stSockAddr));
	if (ret == -1 && timeout > 0) {
		// 建立启动但是尚未完成
		if (errno == EINPROGRESS) {
			while (true) {
				struct pollfd pfd;
				pfd.fd = mFD;
				pfd.events = POLLIN | POLLOUT;
				int rt = poll(&pfd, 1, timeout * 1000000);	// 微妙

				if (rt == -1) {
					if (errno == EINTR) {
					    continue;
					}
					mStatus = wStatus::IOError("wUnixSocket::Connect poll failed", strerror(errno));
					return -1;

				} else if(rt == 0) {
					mStatus = wStatus::IOError("wUnixSocket::Connect connect timeout", timeout);
					return kSeTimeout;

				} else {
					int val, len = sizeof(int);
					if (getsockopt(mFD, SOL_SOCKET, SO_ERROR, (char*)&val, (socklen_t*)&len) == -1) {
					    mStatus = wStatus::IOError("wUnixSocket::Connect getsockopt SO_ERROR failed", strerror(errno));
					    return -1;
					}

					if (val > 0) {
					    mStatus = wStatus::IOError("wUnixSocket::Connect connect failed", strerror(errno));
					    return -1;
					}
					break;	// 连接成功
				}
			}
		} else {
			mStatus = wStatus::IOError("wUnixSocket::Connect connect directly failed", strerror(errno));
			return -1;
		}
	}
	
	mStatus = wStatus::Nothing();
	return 0;
}

int wUnixSocket::Accept(struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		mStatus = wStatus::InvalidArgument("wUnixSocket::Accept", "is not listen socket");
		return -1;
	}
	
	int fd = 0;
	while (true) {
		fd = accept(mFD, clientaddr, addrsize);
		if (fd == -1) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}

			mStatus = wStatus::IOError("wUnixSocket::Accept, accept failed", strerror(errno));
			return -1;
	    }
	}

	return fd;
}

}	// namespace hnet
