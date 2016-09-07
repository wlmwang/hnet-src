
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

wStatus wUnixSocket::Connect(int64_t *ret, string host, uint16_t port, float timeout) {
	mHost = host;

	string tmpsock = "unix_";
	misc::AppendNumberTo(&tmpsock, static_cast<uint64_t>(getpid()));
	tmpsock += ".sock";
	if (!Bind(mHost).Ok()) {
		*ret = static_cast<int64_t>(-1);
		return mStatus;
	}

	struct sockaddr_un stSockAddr;
	memset(&stSockAddr, 0, sizeof(stSockAddr));
	stSockAddr.sun_family = AF_UNIX;
	strncpy(stSockAddr.sun_path, mHost.c_str(), sizeof(stSockAddr.sun_path) - 1);

	// 超时设置
	if (timeout > 0) {
		if (!SetFL().Ok()) {
			*ret = static_cast<int64_t>(-1);
			return mStatus;
		}
	}

	*ret = static_cast<int64_t>(connect(mFD, (const struct sockaddr *)&stSockAddr, sizeof(stSockAddr)));
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
					return mStatus = wStatus::IOError("wUnixSocket::Connect poll failed", strerror(errno));
				} else if(rt == 0) {
					*ret = static_cast<int64_t>(kSeTimeout);
					return mStatus = wStatus::IOError("wUnixSocket::Connect connect timeout", timeout);
				} else {
					int val, len = sizeof(int);
					if (getsockopt(mFD, SOL_SOCKET, SO_ERROR, (char*)&val, (socklen_t*)&len) == -1) {
					    return mStatus = wStatus::IOError("wUnixSocket::Connect getsockopt SO_ERROR failed", strerror(errno));
					}
					if (val > 0) {
					    return mStatus = wStatus::IOError("wUnixSocket::Connect connect failed", strerror(errno));
					}

					// 连接成功
					*ret = 0;
					break;
				}
			}
		} else {
			return mStatus = wStatus::IOError("wUnixSocket::Connect connect directly failed", strerror(errno));
		}
	}
	
	return mStatus = wStatus::Nothing();
}

wStatus wUnixSocket::Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		*fd = -1;
		return mStatus = wStatus::InvalidArgument("wUnixSocket::Accept", "is not listen socket");
	}
	
	while (true) {
		*fd = static_cast<int64_t>(accept(mFD, clientaddr, addrsize));
		if (*fd > 0) {
			mStatus = wStatus::Nothing();
			break;
		} else if (errno == EAGAIN) {
			mStatus = wStatus::Nothing();
			break;
		} else if (errno == EINTR) {
            // 操作被信号中断，中断后唤醒继续处理
            // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
			continue;
		} else {
			mStatus = wStatus::IOError("wUnixSocket::Accept, accept failed", strerror(errno));
			break;
		}
	}
	
	return mStatus;
}

}	// namespace hnet
