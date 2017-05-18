
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include "wTcpSocket.h"
#include "wMisc.h"

namespace hnet {

const wStatus& wTcpSocket::Open() {
	if ((mFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		return mStatus = wStatus::AccessIllegal("wTcpSocket::Open socket() AF_INET failed", error::Strerror(errno));
	}

	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Open setsockopt() SO_REUSEADDR failed", error::Strerror(errno));
	}

	struct linger l = {0, 0};	// 优雅断开
	if (setsockopt(mFD, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Open setsockopt() SO_LINGER failed", error::Strerror(errno));
	}

	// 启用保活机制
	if (mIsKeepAlive) {
		return SetKeepAlive(kKeepAliveTm, kKeepAliveTm, kKeepAliveCnt);
	}
	return mStatus;
}

const wStatus& wTcpSocket::Bind(const std::string& host, uint16_t port) {
	struct sockaddr_in socketAddr;
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons(static_cast<short int>(port));
	socketAddr.sin_addr.s_addr = inet_addr(host.c_str());
	if (bind(mFD, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Bind bind failed", error::Strerror(errno));
	}
	return mStatus;
}

const wStatus& wTcpSocket::Listen(const std::string& host, uint16_t port) {
	mHost = host;
	mPort = port;
	if (!Bind(host, port).Ok()) {
		return mStatus;
	}
	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void *>(&optVal), sizeof(socklen_t)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Listen setsockopt() SO_SNDBUF failed", error::Strerror(errno));
	}
	
	if (listen(mFD, kListenBacklog) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Listen listen failed", error::Strerror(errno));
	}
	return SetFL();
}

const wStatus& wTcpSocket::Connect(int64_t *ret, const std::string& host, uint16_t port, float timeout) {
	mHost = host;
	mPort = port;
	// 超时设置
	if (timeout > 0) {
		if (!SetFL().Ok()) {
			*ret = -1;
			return mStatus;
		}
	}
	
	struct sockaddr_in stSockAddr;
	memset(&stSockAddr, 0, sizeof(sockaddr_in));
	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(static_cast<unsigned short>(port));
	stSockAddr.sin_addr.s_addr = inet_addr(host.c_str());
	*ret = static_cast<int64_t>(connect(mFD, reinterpret_cast<const struct sockaddr *>(&stSockAddr), sizeof(stSockAddr)));
	if (*ret == -1 && timeout > 0) {
		if (errno == EINPROGRESS) {
			// 建立启动但是尚未完成
			while (true) {
				struct pollfd pfd;
				pfd.fd = mFD;
				pfd.events = POLLIN | POLLOUT;
				int rt = poll(&pfd, 1, timeout * 1000000);	// 微妙
				if (rt == -1) {
					if (errno == EINTR) {
					    continue;
					}
					return mStatus = wStatus::IOError("wTcpSocket::Connect poll failed", error::Strerror(errno));
				} else if(rt == 0) {
					return mStatus = wStatus::IOError("wTcpSocket::Connect connect timeout", "");
				} else {
					int error, len = sizeof(int);
					int code = getsockopt(mFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), reinterpret_cast<socklen_t*>(&len));
					if (code == -1) {
					    return mStatus = wStatus::IOError("wTcpSocket::Connect getsockopt SO_ERROR failed", error::Strerror(errno));
					}
					if (error != 0) {
						errno = error;
					    return mStatus = wStatus::IOError("wTcpSocket::Connect connect failed", error::Strerror(errno));
					}
					// 连接成功
					*ret = 0;
					break;
				}
			}
		} else {
			return mStatus = wStatus::IOError("wTcpSocket::Connect connect directly failed", error::Strerror(errno));
		}
	}
	
	socklen_t optVal = 100*1024;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void *>(&optVal), sizeof(socklen_t)) == -1) {
		*ret = -1;
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_SNDBUF failed", error::Strerror(errno));
	}
	return mStatus;
}

const wStatus& wTcpSocket::Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		*fd = -1;
		return mStatus = wStatus::InvalidArgument("wTcpSocket::Accept", "is not listen socket");
	}

	while (true) {
		*fd = static_cast<int64_t>(accept(mFD, clientaddr, addrsize));
		if (*fd > 0) {
			break;
		} else if (errno == EAGAIN) {
			continue;
		} else if (errno == EINTR) {
		    // 操作被信号中断，中断后唤醒继续处理
		    // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
		    continue;
		} else {
		    mStatus = wStatus::IOError("wSocket::Accept, accept failed", "");
		    break;
		}
	}

	if (mStatus.Ok() && *fd > 0) {
		// 设置发送缓冲大小3M
		socklen_t optVal = 0x300000;
		if (setsockopt(static_cast<int>(*fd), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
			mStatus = wStatus::IOError("wTcpSocket::Accept setsockopt() SO_SNDBUF failed", error::Strerror(errno));
		}
	} else {
		mStatus = wStatus::IOError("wTcpSocket::Accept accept() failed", "");
	}
	return mStatus;
}

const wStatus& wTcpSocket::SetTimeout(float timeout) {
	if (SetSendTimeout(timeout).Ok()) {
		return mStatus;
	}
	return SetRecvTimeout(timeout);
}

const wStatus& wTcpSocket::SetSendTimeout(float timeout) {
	struct timeval tv;
	tv.tv_sec = (int)timeout>=0 ? (int)timeout : 0;
	tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
	if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
		tv.tv_sec = 30;
		tv.tv_usec = 0;
	}

	if (setsockopt(mFD, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const void*>(&tv), sizeof(tv)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_SNDTIMEO failed", error::Strerror(errno));
	}
	return mStatus.Clear();
}

const wStatus& wTcpSocket::SetRecvTimeout(float timeout) {
	struct timeval tv;
	tv.tv_sec = static_cast<int>(timeout) >= 0? static_cast<int>(timeout): 0;
	tv.tv_usec = static_cast<int>((timeout - static_cast<int>(timeout)) * 1000000);
	if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
		tv.tv_sec = 30;
		tv.tv_usec = 0;
	}
	
	if (setsockopt(mFD, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const void*>(&tv), sizeof(tv)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_RCVTIMEO failed", error::Strerror(errno));
	}
	return mStatus.Clear();
}

const wStatus& wTcpSocket::SetKeepAlive(int idle, int intvl, int cnt) {
	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const void*>(&flags), sizeof(flags)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_KEEPALIVE failed", error::Strerror(errno));
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_KEEPIDLE failed", error::Strerror(errno));
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_KEEPINTVL failed", error::Strerror(errno));
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_KEEPCNT failed", error::Strerror(errno));
	}

	// Linux Kernel 2.6.37
	// 如果发送出去的数据包在十秒内未收到ACK确认，则下一次调用send或者recv，则函数会返回-1，errno设置为ETIMEOUT
#	ifdef TCP_USER_TIMEOUT
	unsigned int timeout = 10000;
	if (setsockopt(mFD, IPPROTO_TCP, TCP_USER_TIMEOUT, reinterpret_cast<const void*>(&timeout), sizeof(timeout)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_USER_TIMEOUT failed", error::Strerror(errno));
	}
#	endif

	return mStatus.Clear();
}

}	// namespace hnet
