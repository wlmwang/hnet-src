
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include "wTcpSocket.h"

namespace hnet {

const wStatus& wTcpSocket::Open() {
	if ((mFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::AccessIllegal("wTcpSocket::Open socket() AF_INET failed", err);
	}

	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Open setsockopt() SO_REUSEADDR failed", err);
	}

	struct linger l = {0, 0};	// 优雅断开
	if (setsockopt(mFD, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Open setsockopt() SO_LINGER failed", err);
	}

	// 启用保活机制
	if (mIsKeepAlive) {
		return SetKeepAlive(kKeepAliveTm, kKeepAliveTm, kKeepAliveCnt);
	}
	return mStatus.Clear();
}

const wStatus& wTcpSocket::Bind(const std::string& host, uint16_t port) {
	mHost = host;
	mPort = port;

	struct sockaddr_in socketAddr;
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons((short)mPort);
	socketAddr.sin_addr.s_addr = inet_addr(mHost.c_str());

	if (bind(mFD, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Bind bind failed", "");
	}
	return mStatus.Clear();
}

const wStatus& wTcpSocket::Listen(const std::string& host, uint16_t port) {
	if (!Bind(host, port).Ok()) {
		return mStatus;
	}

	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void *>(&optVal), sizeof(socklen_t)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Listen setsockopt() SO_SNDBUF failed", err);
	}
	
	if (listen(mFD, kListenBacklog) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Listen listen failed", err);
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
	stSockAddr.sin_port = htons((unsigned short)port);
	stSockAddr.sin_addr.s_addr = inet_addr(host.c_str());

	*ret = static_cast<int64_t>(::connect(mFD, reinterpret_cast<const struct sockaddr *>(&stSockAddr), sizeof(stSockAddr)));
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
					return mStatus = wStatus::IOError("wTcpSocket::Connect poll failed", err);
				} else if(rt == 0) {
					return mStatus = wStatus::IOError("wTcpSocket::Connect connect timeout", "");
				} else {
					int val, len = sizeof(int);
					if (getsockopt(mFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&val), reinterpret_cast<socklen_t*>(&len)) == -1) {
				    	char err[kMaxErrorLen];
				    	::strerror_r(errno, err, kMaxErrorLen);
					    return mStatus = wStatus::IOError("wTcpSocket::Connect getsockopt SO_ERROR failed", err);
					}
					if (val > 0) {
				    	char err[kMaxErrorLen];
				    	::strerror_r(errno, err, kMaxErrorLen);
					    return mStatus = wStatus::IOError("wTcpSocket::Connect connect failed", err);
					}

					// 连接成功
					*ret = 0;
					break;
				}
			}
		} else {
	    	char err[kMaxErrorLen];
	    	::strerror_r(errno, err, kMaxErrorLen);
			return mStatus = wStatus::IOError("wTcpSocket::Connect connect directly failed", err);
		}
	}
	
	socklen_t optVal = 100*1024;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void *>(&optVal), sizeof(socklen_t)) == -1) {
		*ret = -1;
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_SNDBUF failed", err);
	}
	
	return mStatus.Clear();
}

const wStatus& wTcpSocket::Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		*fd = -1;
		return mStatus = wStatus::InvalidArgument("wTcpSocket::Accept", "is not listen socket");
	}

	while (true) {
		*fd = static_cast<int64_t>(accept(mFD, clientaddr, addrsize));
		if (*fd > 0) {
			mStatus.Clear();
			break;
		} else if (errno == EAGAIN) {
	    	char err[kMaxErrorLen];
	    	::strerror_r(errno, err, kMaxErrorLen);
			mStatus = wStatus::IOError("wTcpSocket::Accept accept() failed", err, false);
			break;
		} else if (errno == EINTR) {
		    // 操作被信号中断，中断后唤醒继续处理
		    // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
		    continue;
		} else {
	    	char err[kMaxErrorLen];
	    	::strerror_r(errno, err, kMaxErrorLen);
		    mStatus = wStatus::IOError("wSocket::Accept, accept failed", err);
		    break;
		}
	}

	if (mStatus.Ok() && *fd > 0) {
		// 设置发送缓冲大小3M
		socklen_t optVal = 0x300000;
		if (setsockopt(static_cast<int>(*fd), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
	    	char err[kMaxErrorLen];
	    	::strerror_r(errno, err, kMaxErrorLen);
			mStatus = wStatus::IOError("wTcpSocket::Accept setsockopt() SO_SNDBUF failed", err);
		}
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
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_SNDTIMEO failed", err);
	}
	return mStatus.Clear();
}

const wStatus& wTcpSocket::SetRecvTimeout(float timeout) {
	struct timeval tv;
	tv.tv_sec = (int)timeout>=0 ? (int)timeout : 0;
	tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
	if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
		tv.tv_sec = 30;
		tv.tv_usec = 0;
	}
	
	if (setsockopt(mFD, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const void*>(&tv), sizeof(tv)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_RCVTIMEO failed", err);
	}
	return mStatus.Clear();
}

const wStatus& wTcpSocket::SetKeepAlive(int idle, int intvl, int cnt) {
	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const void*>(&flags), sizeof(flags)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_KEEPALIVE failed", err);
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_KEEPIDLE failed", err);
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_KEEPINTVL failed", err);
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_KEEPCNT failed", err);
	}

	// Linux Kernel 2.6.37
	// 如果发送出去的数据包在十秒内未收到ACK确认，则下一次调用send或者recv，则函数会返回-1，errno设置为ETIMEOUT
#	ifdef TCP_USER_TIMEOUT
	unsigned int timeout = 10000;
	if (setsockopt(mFD, IPPROTO_TCP, TCP_USER_TIMEOUT, reinterpret_cast<const void*>(&timeout), sizeof(timeout)) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
		return mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() TCP_USER_TIMEOUT failed", err);
	}
#	endif

	return mStatus.Clear();
}

}	// namespace hnet
