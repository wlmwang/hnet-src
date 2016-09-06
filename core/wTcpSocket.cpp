
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMisc.h"
#include "wTcpSocket.h"

namespace hnet {

wStatus wTcpSocket::Open() {
	if ((mFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Open socket() failed", strerror(errno));
	}

	int flags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Open setsockopt() SO_REUSEADDR failed", strerror(errno));
	}

	struct linger stLing = {0, 0};	// 优雅断开
	if (setsockopt(mFD, SOL_SOCKET, SO_LINGER, &stLing, sizeof(stLing)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Open setsockopt() SO_LINGER failed", strerror(errno));
	}

	// 启用保活机制
	if (mIsKeepAlive) {
		return SetKeepAlive(kKeepAliveTm, kKeepAliveTm, kKeepAliveCnt);
	}
	return mStatus = wStatus::Nothing();
}

wStatus wTcpSocket::Bind(string host, uint16_t port) {
	mHost = host;
	mPort = port;

	struct sockaddr_in stSocketAddr;
	stSocketAddr.sin_family = AF_INET;
	stSocketAddr.sin_port = htons((short)mPort);
	stSocketAddr.sin_addr.s_addr = inet_addr(mHost.c_str());

	if (bind(mFD, (struct sockaddr *)&stSocketAddr, sizeof(stSocketAddr)) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Bind bind failed", strerror(errno));
	}
	return mStatus = wStatus::Nothing();
}

wStatus wTcpSocket::Listen(string host, uint16_t port) {
	if (!Bind(host, port).Ok()) {
		return mStatus;
	}

	// 设置发送缓冲大小4M
	int iOptLen = sizeof(socklen_t);
	int iOptVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, (const void *)&iOptVal, iOptLen) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Listen setsockopt() SO_SNDBUF failed", strerror(errno));
	}
	
	if (listen(mFD, kListenBacklog) == -1) {
		return mStatus = wStatus::IOError("wTcpSocket::Listen listen failed", strerror(errno));
	}

	return SetFL();
}

int wTcpSocket::Connect(string host, uint16_t port, float timeout) {
	mHost = host;
	mPort = port;

	struct sockaddr_in stSockAddr;
	memset(&stSockAddr, 0, sizeof(sockaddr_in));
	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons((short)mPort);
	stSockAddr.sin_addr.s_addr = inet_addr(mHost.c_str());

	socklen_t iOptVal = 100*1024;
	socklen_t iOptLen = sizeof(socklen_t);
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, (const void *)&iOptVal, iOptLen) == -1) {
		mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_SNDBUF failed", strerror(errno));
		return -1;
	}

	// 超时设置
	if (timeout > 0) {
		if (SetFL().Ok()) {	// todo
			SetSendTimeout(timeout);	// linux平台下可用
		}
	}

	int ret = connect(mFD, (const struct sockaddr *)&stSockAddr, sizeof(stSockAddr));
	if (timeout > 0 && ret == -1) {
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
                    mStatus = wStatus::IOError("wTcpSocket::Connect poll failed", strerror(errno));
                    return -1;

                } else if(rt == 0) {
                    mStatus = wStatus::IOError("wTcpSocket::Connect connect timeout", timeout);
                    return kSeTimeout;

                } else {
                	int val, len = sizeof(int);
                    if (getsockopt(mFD, SOL_SOCKET, SO_ERROR, (char*)&val, (socklen_t*)&len) == -1) {
                        mStatus = wStatus::IOError("wTcpSocket::Connect getsockopt SO_ERROR failed", strerror(errno));
                        return -1;
                    }

                    if (val > 0) {
                        mStatus = wStatus::IOError("wTcpSocket::Connect connect failed", strerror(errno));
                        return -1;
                    }
                    break;	// 连接成功
                }
			}
		} else {
			mStatus = wStatus::IOError("wTcpSocket::Connect connect directly failed", strerror(errno));
			return -1;
		}
	}

	mStatus = wStatus::Nothing();
	return 0;
}

int wTcpSocket::Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize) {
	if (mSockType != kStListen) {
		return -1;
	}

	int fd = 0;
	while (true) {
		fd = accept(mFD, pClientSockAddr, pSockAddrSize);
		if (fd == -1) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}

			mStatus = wStatus::IOError("wTcpSocket::Accept, accept failed", strerror(errno));
			return -1;
	    }
	}

	// 设置发送缓冲大小3M
	int iOptLen = sizeof(socklen_t);
	int iOptVal = 0x300000;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const void *)&iOptVal, iOptLen) == -1) {
		mStatus = wStatus::IOError("wTcpSocket::Connect setsockopt() SO_SNDBUF failed", strerror(errno));
		return -1;
	}
	return fd;
}

int wTcpSocket::SetTimeout(float fTimeout)
{
	if (SetSendTimeout(fTimeout) < 0) {
		return -1;
	}
	if (SetRecvTimeout(fTimeout) < 0) {
		return -1;
	}
	return 0;
}

int wTcpSocket::SetSendTimeout(float fTimeout) {
	if (mFD == FD_UNKNOWN) {
		return -1;
	}

	struct timeval stTimetv;
	stTimetv.tv_sec = (int)fTimeout>=0 ? (int)fTimeout : 0;
	stTimetv.tv_usec = (int)((fTimeout - (int)fTimeout) * 1000000);
	if (stTimetv.tv_usec < 0 || stTimetv.tv_usec >= 1000000 || (stTimetv.tv_sec == 0 && stTimetv.tv_usec == 0)) {
		stTimetv.tv_sec = 30;
		stTimetv.tv_usec = 0;
	}

	if (setsockopt(mFD, SOL_SOCKET, SO_SNDTIMEO, &stTimetv, sizeof(stTimetv)) == -1) {
    	mErr = errno;
        return -1;  
    }
    return 0;
}

int wTcpSocket::SetRecvTimeout(float fTimeout) {
	if (mFD == FD_UNKNOWN) {
		return -1;
	}

	struct timeval stTimetv;
	stTimetv.tv_sec = (int)fTimeout>=0 ? (int)fTimeout : 0;
	stTimetv.tv_usec = (int)((fTimeout - (int)fTimeout) * 1000000);
	if (stTimetv.tv_usec < 0 || stTimetv.tv_usec >= 1000000 || (stTimetv.tv_sec == 0 && stTimetv.tv_usec == 0)) {
		stTimetv.tv_sec = 30;
		stTimetv.tv_usec = 0;
	}
	
	if (setsockopt(mFD, SOL_SOCKET, SO_RCVTIMEO, &stTimetv, sizeof(stTimetv)) == -1) {
    	mErr = errno;
        return -1;  
    }
    return 0;
}

int wTcpSocket::SetKeepAlive(int iIdle, int iIntvl, int iCnt) {
	if (mFD == FD_UNKNOWN) {
		return -1;
	}

	int iFlags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_KEEPALIVE, &iFlags, sizeof(iFlags)) == -1) {
    	mErr = errno;
        return -1;  
    }
	if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPIDLE, &iIdle, sizeof(iIdle)) == -1) {
    	mErr = errno;
        return -1;  
    }
	if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPINTVL, &iIntvl, sizeof(iIntvl)) == -1) {
    	mErr = errno;
        return -1;  
    }
	if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPCNT, &iCnt, sizeof(iCnt)) == -1) {
    	mErr = errno;
        return -1;  
    }

    //Linux Kernel 2.6.37
    //如果发送出去的数据包在十秒内未收到ACK确认，则下一次调用send或者recv，则函数会返回-1，errno设置为ETIMEOUT
#ifdef TCP_USER_TIMEOUT
    unsigned int iTimeout = 10000;
	if (setsockopt(mFD, IPPROTO_TCP, TCP_USER_TIMEOUT, &iTimeout, sizeof(iTimeout)) == -1) {
    	mErr = errno;
        return -1;
    }
#endif
    
    return 0;
}

}	// namespace hnet
