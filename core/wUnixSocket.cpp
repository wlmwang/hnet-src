
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/un.h>
#include <poll.h>
#include "wUnixSocket.h"
#include "wMisc.h"
#include "wEnv.h"

namespace hnet {

wUnixSocket::~wUnixSocket() {
	wEnv::Default()->DeleteFile(mHost);
}

int wUnixSocket::Open() {
	mFD = socket(AF_UNIX, SOCK_STREAM, 0);
	if (mFD == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Open socket() failed", error::Strerror(errno).c_str());
		return -1;
	}

	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Open setsockopt(SO_REUSEADDR) failed", error::Strerror(errno).c_str());
		return -1;
	}
	
	// 优雅断开
	// 底层将未发送完的数据发送完成后再释放资源
	struct linger l = {0, 0};
	if (setsockopt(mFD, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Open setsockopt(SO_LINGER) failed", error::Strerror(errno).c_str());
		return -1;
	}
	return 0;
}

int wUnixSocket::Bind(const std::string& host, uint16_t port) {
	struct sockaddr_un socketAddr;
	memset(&socketAddr, 0, sizeof(socketAddr));
	socketAddr.sun_family = AF_UNIX;
	strncpy(socketAddr.sun_path, host.c_str(), sizeof(socketAddr.sun_path) - 1);
	int ret = bind(mFD, reinterpret_cast<struct sockaddr*>(&socketAddr), sizeof(socketAddr));
	if (ret == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Bind bind() failed", error::Strerror(errno).c_str());
	}
	return ret;
}

int wUnixSocket::Listen(const std::string& host, uint16_t port) {
	mHost = host;
	mPort = port;

	if (Bind(mHost, mPort) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Listen Bind() failed", "");
		return -1;
	}

	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Listen setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
		return -1;
	}

	if (listen(mFD, kListenBacklog) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Listen listen() failed", error::Strerror(errno).c_str());
		return -1;
	}
	
	if (SetNonblock() == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Listen SetNonblock() failed", "");
		return -1;
	}
	return 0;
}

int wUnixSocket::Connect(const std::string& host, uint16_t port, float timeout) {
	char filename[PATH_MAX];
	snprintf(filename, PATH_MAX, "%s%d%s", kUnixSockPrefix, static_cast<int>(getpid()), ".sock");
	mHost = filename;
	mPort = 0;

	if (Bind(mHost, mPort) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect Bind() failed", "");
		return -1;
	}

	// 设置非阻塞标志
	int oldfl = GetNonblock();
	if (oldfl == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect GetNonblock() failed", "");
		return -1;
	}

	if (timeout > 0 && oldfl == 0) {	// 阻塞
		if (SetNonblock() == -1) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect SetNonblock() failed", "");
			return -1;
		}
	}

	struct sockaddr_un socketAddr;
	memset(&socketAddr, 0, sizeof(socketAddr));
	socketAddr.sun_family = AF_UNIX;
	strncpy(socketAddr.sun_path, host.c_str(), sizeof(socketAddr.sun_path) - 1);

	int ret = static_cast<int64_t>(connect(mFD, reinterpret_cast<const struct sockaddr *>(&socketAddr), sizeof(socketAddr)));
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
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect poll() failed", error::Strerror(errno).c_str());
					ret = -1;
					break;

				} else if (r == 0) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect poll() failed", "connect timeout");
					errno = ETIMEDOUT;
					ret = -1;
					break;

				} else {
					int error, len = sizeof(int);
					int code = getsockopt(mFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), reinterpret_cast<socklen_t*>(&len));
					if (code == -1) {
					    LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect setsockopt() failed", error::Strerror(errno).c_str());
						ret = -1;
						break;
					}

					if (error != 0) {
					    LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect connect() failed", error::Strerror(errno).c_str());
						errno = error;
						ret = -1;
						break;
					}

					ret = 0;	// 连接成功
					break;
				}
			}
		} else {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect connect() directly failed", error::Strerror(errno).c_str());
		}
	}

	// 还原非阻塞状态
	if (timeout > 0 && oldfl == 0) {
		if (SetNonblock(false) == -1) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect SetNonblock() failed", "");
			return -1;
		}
	}

	// 设置发送缓冲大小4M
	socklen_t optVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Connect setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
		return -1;
	}
	return ret;
}

int wUnixSocket::Accept(int* fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
	if (mSockType != kStListen) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Accept () failed", "error st");
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
		    LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Accept accept() failed", error::Strerror(errno).c_str());
		    ret = -1;
		    break;
		}
	}

	if (*fd > 0) {
		// 设置发送缓冲大小4M
		socklen_t optVal = 0x400000;
		if (setsockopt(static_cast<int>(*fd), SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const void*>(&optVal), sizeof(socklen_t)) == -1) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::Accept setsockopt(SO_SNDBUF) failed", error::Strerror(errno).c_str());
			return -1;
		}
	}
	return ret;
}

int wUnixSocket::SetTimeout(float timeout) {
	if (SetSendTimeout(timeout) == -1) {
		return -1;
	}
	return SetRecvTimeout(timeout);
}

int wUnixSocket::SetSendTimeout(float timeout) {
	struct timeval tv;
	tv.tv_sec = (int)timeout>=0 ? (int)timeout : 0;
	tv.tv_usec = (int)((timeout - (int)timeout) * 1000000);
	if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
		tv.tv_sec = 30;
		tv.tv_usec = 0;
	}
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const void*>(&tv), sizeof(tv)) == -1) {
		LOG_ERROR(soft::GetLogPath(), "%s : %s", "wUnixSocket::SetSendTimeout setsockopt(SO_SNDTIMEO) failed", error::Strerror(errno).c_str());
		return -1;
	}
	return 0;
}

int wUnixSocket::SetRecvTimeout(float timeout) {
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

}	// namespace hnet
