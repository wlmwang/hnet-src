
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

int wTcpSocket::Open() {
	mFD = socket(AF_INET, SOCK_STREAM, 0);
	if (mFD == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Open socket() failed", error::Strerror(errno).c_str());
		return -1;
	}

	// 地址重用
	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Open setsockopt(SO_REUSEADDR) failed", error::Strerror(errno).c_str());
		return -1;
	}

	// 优雅断开
	// 底层将未发送完的数据发送完成后再释放资源
	struct linger l = {0, 0};
	if (setsockopt(mFD, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Open setsockopt(SO_LINGER) failed", error::Strerror(errno).c_str());
		return -1;
	}

	// 启用保活机制
	if (mIsKeepAlive) {
		return SetKeepAlive(kKeepAliveTm, kKeepAliveTm, kKeepAliveCnt);
	}
	return 0;
}

int wTcpSocket::Bind(const std::string& host, uint16_t port) {
	struct sockaddr_in socketAddr;
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons(static_cast<short int>(port));
	socketAddr.sin_addr.s_addr = misc::Text2IP(host.c_str());
	int ret = bind(mFD, reinterpret_cast<struct sockaddr *>(&socketAddr), sizeof(socketAddr));
	if (ret == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Bind bind() failed", error::Strerror(errno).c_str());
	}
	return ret;
}

int wTcpSocket::Listen(const std::string& host, uint16_t port) {
	mHost = host;
	mPort = port;

	if (Bind(mHost, mPort) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Listen Bind() failed", "");
		return -1;
	}

	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Listen setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
		return -1;
	}
	
	if (listen(mFD, kListenBacklog) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Listen listen() failed", error::Strerror(errno).c_str());
		return -1;
	}
	
	if (SetNonblock() == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Listen SetNonblock() failed", "");
		return -1;
	}
	return 0;
}

int wTcpSocket::Connect(const std::string& host, uint16_t port, float timeout) {
	mHost = host;
	mPort = port;

	// 设置非阻塞标志
	int oldfl = GetNonblock();
	if (oldfl == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect GetNonblock() failed", "");
		return -1;
	}

	if (timeout > 0 && oldfl == 0) {	// 阻塞
		if (SetNonblock() == -1) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect SetNonblock() failed", "");
			return -1;
		}
	}
	
	struct sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(sockaddr_in));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(static_cast<unsigned short>(port));
	sockAddr.sin_addr.s_addr = misc::Text2IP(host.c_str());

	int ret = static_cast<int64_t>(connect(mFD, reinterpret_cast<const struct sockaddr*>(&sockAddr), sizeof(sockAddr)));
	if (ret == -1 && timeout > 0) {
		if (errno == EINPROGRESS) {	// 建立启动但是尚未完成
			while (true) {
				struct pollfd pll;
				pll.fd = mFD;
				pll.events = POLLIN | POLLOUT;

				int r = poll(&pll, 1, timeout*1000000);	// 微妙
				if (r == -1) {
					if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {	// Interrupted system call
					    continue;
					}
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect poll() failed", error::Strerror(errno).c_str());
					ret = -1;
					break;

				} else if (r == 0) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect poll() failed", "connect timeout");
					errno = ETIMEDOUT;
					ret = -1;
					break;

				} else {
					int error, len = sizeof(int);
					int code = getsockopt(mFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), reinterpret_cast<socklen_t*>(&len));
					if (code == -1) {
					    LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect setsockopt() failed", error::Strerror(errno).c_str());
						ret = -1;
						break;
					}

					if (error != 0) {
					    LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect connect() failed", error::Strerror(errno).c_str());
						errno = error;
						ret = -1;
						break;
					}

					ret = 0;	// 连接成功
					break;
				}
			}
		} else {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect connect() directly failed", error::Strerror(errno).c_str());
		}
	}
	
	// 还原非阻塞状态
	if (timeout > 0 && oldfl == 0) {
		if (SetNonblock(false) == -1) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect SetNonblock() failed", "");
			return -1;
		}
	}

	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Connect setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
		return -1;
	}
	return ret;
}

int wTcpSocket::Accept(int* fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Accept () failed", "error st");
		return *fd = -1;
	}

	int ret = 0;
	while (true) {
		*fd = accept(mFD, clientaddr, addrsize);
		if (*fd > 0) {
			break;
		} else if (errno == EAGAIN || errno == EWOULDBLOCK) {	// Resource temporarily unavailable // 资源暂时不够(可能写缓冲区满)
            //continue;
            ret = 0;
            break;
		} else if (errno == EINTR) {	// Interrupted system call
            //continue;
            ret = 0;
            break;
		} else {
		    LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Accept accept() failed", error::Strerror(errno).c_str());
		    ret = -1;
		    break;
		}
	}

	if (*fd > 0) {
		// 设置发送缓冲大小4M
		socklen_t optVal = 0x400000;
		if (setsockopt(static_cast<int>(*fd), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::Accept setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
			return -1;
		}
	}
	return ret;
}

int wTcpSocket::SetTimeout(float timeout) {
	if (SetSendTimeout(timeout) == -1) {
		return -1;
	}
	return SetRecvTimeout(timeout);
}

int wTcpSocket::SetSendTimeout(float timeout) {
	struct timeval tv;
	tv.tv_sec = (int)timeout>=0 ? (int)timeout : 0;
	tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
	if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
		tv.tv_sec = 30;
		tv.tv_usec = 0;
	}
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const void*>(&tv), sizeof(tv)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetSendTimeout setsockopt(SO_SNDTIMEO) failed", error::Strerror(errno).c_str());
		return -1;
	}
	return 0;
}

int wTcpSocket::SetRecvTimeout(float timeout) {
	struct timeval tv;
	tv.tv_sec = static_cast<int>(timeout) >= 0? static_cast<int>(timeout): 0;
	tv.tv_usec = static_cast<int>((timeout - static_cast<int>(timeout)) * 1000000);
	if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
		tv.tv_sec = 30;
		tv.tv_usec = 0;
	}
	if (setsockopt(mFD, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const void*>(&tv), sizeof(tv)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetRecvTimeout setsockopt(SO_RCVTIMEO) failed", error::Strerror(errno).c_str());
		return -1;
	}
	return 0;
}

int wTcpSocket::SetKeepAlive(int idle, int intvl, int cnt) {
#ifdef SO_KEEPALIVE
	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const void*>(&flags), sizeof(flags)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetKeepAlive setsockopt(SO_KEEPALIVE) failed", error::Strerror(errno).c_str());
		return -1;
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetKeepAlive setsockopt(TCP_KEEPIDLE) failed", error::Strerror(errno).c_str());
		return -1;
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetKeepAlive setsockopt(TCP_KEEPINTVL) failed", error::Strerror(errno).c_str());
		return -1;
	} else if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetKeepAlive setsockopt(TCP_KEEPCNT) failed", error::Strerror(errno).c_str());
		return -1;
	}
#endif

	// Linux Kernel 2.6.37
#ifdef TCP_USER_TIMEOUT

	// 如果发送出去的数据包在10秒内未收到ACK确认，则下一次调用send或者recv，则函数会返回-1，errno设置为ETIMEOUT
	unsigned int timeout = 10000;
	if (setsockopt(mFD, IPPROTO_TCP, TCP_USER_TIMEOUT, reinterpret_cast<const void*>(&timeout), sizeof(timeout)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTcpSocket::SetKeepAlive setsockopt(TCP_USER_TIMEOUT) failed", error::Strerror(errno).c_str());
		return -1;
	}
#endif

	return 0;
}

}	// namespace hnet
