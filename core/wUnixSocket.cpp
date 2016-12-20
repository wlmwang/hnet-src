
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/un.h>
#include <poll.h>
#include "wUnixSocket.h"
#include "wMisc.h"

namespace hnet {

const wStatus& wUnixSocket::Open() {
	if ((mFD = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wUnixSocket::Open socket() AF_UNIX failed", err);
	}
	return mStatus.Clear();
}

const wStatus& wUnixSocket::Bind(const std::string& host, uint16_t port) {
	struct sockaddr_un socketAddr;
	memset(&socketAddr, 0, sizeof(socketAddr));
	socketAddr.sun_family = AF_UNIX;
	strncpy(socketAddr.sun_path, host.c_str(), sizeof(socketAddr.sun_path) - 1);

	if (bind(mFD, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wUnixSocket::Bind bind failed", err);
	}
	return mStatus.Clear();
}

const wStatus& wUnixSocket::Listen(const std::string& host, uint16_t port) {
	mHost = host;
	
	if (!Bind(mHost).Ok()) {
		return mStatus;
	}

	if (listen(mFD, kListenBacklog) < 0) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wUnixSocket::Listen listen failed", err);
	}
	
	return SetFL();
}

const wStatus& wUnixSocket::Connect(int64_t *ret, const std::string& host, uint16_t port, float timeout) {
	mHost = host;

	std::string tmpsock = "unix_";
	logging::AppendNumberTo(&tmpsock, static_cast<uint64_t>(getpid()));
	tmpsock += ".sock";
	if (!Bind(mHost).Ok()) {
		*ret = static_cast<int64_t>(-1);
		return mStatus;
	}
	
	// 超时设置
	if (timeout > 0) {
		if (!SetFL().Ok()) {
			*ret = -1;
			return mStatus;
		}
	}
	
	struct sockaddr_un socketAddr;
	memset(&socketAddr, 0, sizeof(socketAddr));
	socketAddr.sun_family = AF_UNIX;
	strncpy(socketAddr.sun_path, mHost.c_str(), sizeof(socketAddr.sun_path) - 1);

	*ret = static_cast<int64_t>(connect(mFD, reinterpret_cast<const struct sockaddr *>(&socketAddr), sizeof(socketAddr)));
	if (*ret == -1 && timeout > 0) {
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
			    	char err[kMaxErrorLen];
			    	::strerror_r(errno, err, kMaxErrorLen);
					return mStatus = wStatus::IOError("wUnixSocket::Connect poll failed", err);
				} else if(rt == 0) {
			    	char err[kMaxErrorLen];
			    	::strerror_r(errno, err, kMaxErrorLen);
					return mStatus = wStatus::IOError("wUnixSocket::Connect connect timeout", "");
				} else {
					int val, len = sizeof(int);
					if (getsockopt(mFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&val), reinterpret_cast<socklen_t*>(&len)) == -1) {
				    	char err[kMaxErrorLen];
				    	::strerror_r(errno, err, kMaxErrorLen);
					    return mStatus = wStatus::IOError("wUnixSocket::Connect getsockopt SO_ERROR failed", err);
					}
					if (val > 0) {
				    	char err[kMaxErrorLen];
				    	::strerror_r(errno, err, kMaxErrorLen);
					    return mStatus = wStatus::IOError("wUnixSocket::Connect connect failed", err);
					}

					// 连接成功
					*ret = 0;
					break;
				}
			}
		} else {
	    	char err[kMaxErrorLen];
	    	::strerror_r(errno, err, kMaxErrorLen);
			return mStatus = wStatus::IOError("wUnixSocket::Connect connect directly failed", err);
		}
	}
	
	return mStatus.Clear();
}

const wStatus& wUnixSocket::Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		*fd = -1;
		return mStatus = wStatus::InvalidArgument("wUnixSocket::Accept", "is not listen socket");
	}
	
	while (true) {
		*fd = static_cast<int64_t>(accept(mFD, clientaddr, addrsize));
		if (*fd > 0) {
			mStatus.Clear();
			break;
		} else if (errno == EAGAIN) {
			mStatus.Clear();
			break;
		} else if (errno == EINTR) {
            // 操作被信号中断，中断后唤醒继续处理
            // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
			continue;
		} else {
	    	char err[kMaxErrorLen];
	    	::strerror_r(errno, err, kMaxErrorLen);
			mStatus = wStatus::IOError("wUnixSocket::Accept, accept failed", err);
			break;
		}
	}
	
	return mStatus;
}

}	// namespace hnet
