
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wTcpSocket.h"

int wTcpSocket::Open()
{
	if ((mFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		mErr = errno;
		return -1;
	}

	int iFlags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_REUSEADDR, &iFlags, sizeof(iFlags)) == -1)	//端口重用
	{
		mErr = errno;
		Close();
		return -1;
	}

	struct linger stLing = {0, 0};
	if (setsockopt(mFD, SOL_SOCKET, SO_LINGER, &stLing, sizeof(stLing)) == -1)	//优雅断开
	{
		mErr = errno;
		Close();
		return -1;
	}

	if (mIsKeepAlive)
	{
		if (SetKeepAlive(KEEPALIVE_TIME, KEEPALIVE_TIME, KEEPALIVE_CNT) < 0)	//启用保活机制
		{
			return -1;
		}
	}
	return mFD;
}

int wTcpSocket::Bind(string sHost, unsigned int nPort)
{
	if (mFD == FD_UNKNOWN)
	{
		return -1;
	}
	mHost = sHost;
	mPort = nPort;

	struct sockaddr_in stSocketAddr;
	stSocketAddr.sin_family = AF_INET;
	stSocketAddr.sin_port = htons((short)nPort);
	stSocketAddr.sin_addr.s_addr = inet_addr(mHost.c_str());

	if (bind(mFD, (struct sockaddr *)&stSocketAddr, sizeof(stSocketAddr)) < 0)
	{
		mErr = errno;
		Close();
		return -1;
	}
	return 0;
}

int wTcpSocket::Listen(string sHost, unsigned int nPort)
{
	if (mFD == FD_UNKNOWN)
	{
		return -1;
	}

	if (Bind(sHost, nPort) < 0)
	{
		mErr = errno;
		Close();
		return -1;
	}

	//设置发送缓冲大小4M
	int iOptLen = sizeof(socklen_t);
	int iOptVal = 0x400000;
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, (const void *)&iOptVal, iOptLen) == -1)
	{
		mErr = errno;
		Close();
		return -1;
	}
	
	if (listen(mFD, LISTEN_BACKLOG) < 0)
	{
		mErr = errno;
		Close();
		return -1;
	}

	if (SetNonBlock() < 0) 
	{
		LOG_ERROR(ELOG_KEY, "[system] Set listen socket nonblock failed, close it");
	}
	return 0;
}

int wTcpSocket::Connect(string sHost, unsigned int nPort, float fTimeout)
{
	if (mFD == FD_UNKNOWN)
	{
		return -1;
	}
	mHost = sHost;
	mPort = nPort;

	struct sockaddr_in stSockAddr;
	memset(&stSockAddr, 0, sizeof(sockaddr_in));
	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons((short)nPort);
	stSockAddr.sin_addr.s_addr = inet_addr(mHost.c_str());;

	socklen_t iOptVal = 100*1024;
	socklen_t iOptLen = sizeof(socklen_t);
	if (setsockopt(mFD, SOL_SOCKET, SO_SNDBUF, (const void *)&iOptVal, iOptLen) == -1)
	{
		mErr = errno;
		Close();
		return -1;
	}

	//超时设置
	if (fTimeout > 0)
	{
		if (SetNonBlock() < 0)
		{
			SetSendTimeout(fTimeout);	//linux平台下可用
		}
	}

	int iRet = connect(mFD, (const struct sockaddr *)&stSockAddr, sizeof(stSockAddr));
	if (fTimeout > 0 && iRet < 0)
	{
		if (errno == EINPROGRESS)	//建立启动但是尚未完成
		{
			int iLen, iVal, iRet;
			struct pollfd stFD;
			int iTimeout = fTimeout * 1000000;	//微妙
			while (true)
			{
				stFD.fd = mFD;
                stFD.events = POLLIN | POLLOUT;
                iRet = poll(&stFD, 1, iTimeout);
                if (iRet == -1)
                {
                	mErr = errno;
                    if(mErr == EINTR)
                    {
                        continue;
                    }
                    Close();
                    return -1;
                }
                else if(iRet == 0)
                {
                    Close();
                    LOG_ERROR(ELOG_KEY, "[system] tcp connect timeout millisecond=%d", iTimeout);
                    return ERR_TIMEOUT;
                }
                else
                {
                    iLen = sizeof(iVal);
                    iRet = getsockopt(mFD, SOL_SOCKET, SO_ERROR, (char*)&iVal, (socklen_t*)&iLen);
                    if(iRet == -1)
                    {
                    	mErr = errno;
                        Close();
                        LOG_ERROR(ELOG_KEY, "[system] ip=%s:%d, tcp connect getsockopt errno=%d,%s", mHost.c_str(), mPort, mErr, strerror(mErr));
                        return -1;
                    }
                    if(iVal > 0)
                    {
                    	mErr = errno;
                        LOG_ERROR(ELOG_KEY, "[system] ip=%s:%d, tcp connect fail errno=%d,%s", mHost.c_str(), mPort, mErr, strerror(mErr));
                        Close();
                        return -1;
                    }
                    break;	//连接成功
                }
			}
		}
		else
		{
			mErr = errno;
            LOG_ERROR(ELOG_KEY, "[system] ip=%s:%d, tcp connect directly errno=%d,%s", mHost.c_str(), mPort, mErr, strerror(mErr));
			Close();
			return -1;
		}
	}
	return 0;
}

int wTcpSocket::Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize)
{
	if (mFD == FD_UNKNOWN || mSockType != SOCK_TYPE_LISTEN)
	{
		return -1;
	}

	int iNewFD = 0;
	do {
		iNewFD = accept(mFD, pClientSockAddr, pSockAddrSize);
		if (iNewFD < 0)
		{
			mErr = errno;
			if (mErr == EINTR)	//中断
			{
				continue;
			}
			if (mErr == EAGAIN || mErr == EWOULDBLOCK)	//没有新连接
			{
				return 0;
			}
			break;
	    }
	    break;
	} while (true);

	if (iNewFD <= 0)
	{
		return -1;
	}

	//设置发送缓冲大小3M
	int iOptLen = sizeof(socklen_t);
	int iOptVal = 0x300000;
	if (setsockopt(iNewFD, SOL_SOCKET, SO_SNDBUF, (const void *)&iOptVal, iOptLen) == -1)
	{
		mErr = errno;
		return -1;
	}
	return iNewFD;
}

int wTcpSocket::SetTimeout(float fTimeout)
{
	if (SetSendTimeout(fTimeout) < 0)
	{
		return -1;
	}
	if (SetRecvTimeout(fTimeout) < 0)
	{
		return -1;
	}
	return 0;
}

int wTcpSocket::SetSendTimeout(float fTimeout)
{
	if (mFD == FD_UNKNOWN) 
	{
		return -1;
	}

	struct timeval stTimetv;
	stTimetv.tv_sec = (int)fTimeout>=0 ? (int)fTimeout : 0;
	stTimetv.tv_usec = (int)((fTimeout - (int)fTimeout) * 1000000);
	if (stTimetv.tv_usec < 0 || stTimetv.tv_usec >= 1000000 || (stTimetv.tv_sec == 0 && stTimetv.tv_usec == 0))
	{
		stTimetv.tv_sec = 30;
		stTimetv.tv_usec = 0;
	}

	if (setsockopt(mFD, SOL_SOCKET, SO_SNDTIMEO, &stTimetv, sizeof(stTimetv)) == -1)  
    {
    	mErr = errno;
        return -1;  
    }
    return 0;
}

int wTcpSocket::SetRecvTimeout(float fTimeout)
{
	if (mFD == FD_UNKNOWN) 
	{
		return -1;
	}

	struct timeval stTimetv;
	stTimetv.tv_sec = (int)fTimeout>=0 ? (int)fTimeout : 0;
	stTimetv.tv_usec = (int)((fTimeout - (int)fTimeout) * 1000000);
	if (stTimetv.tv_usec < 0 || stTimetv.tv_usec >= 1000000 || (stTimetv.tv_sec == 0 && stTimetv.tv_usec == 0))
	{
		stTimetv.tv_sec = 30;
		stTimetv.tv_usec = 0;
	}
	
	if (setsockopt(mFD, SOL_SOCKET, SO_RCVTIMEO, &stTimetv, sizeof(stTimetv)) == -1)  
    {
    	mErr = errno;
        return -1;  
    }
    return 0;
}

int wTcpSocket::SetKeepAlive(int iIdle, int iIntvl, int iCnt)
{
	if (mFD == FD_UNKNOWN) 
	{
		return -1;
	}

	int iFlags = 1;
	if (setsockopt(mFD, SOL_SOCKET, SO_KEEPALIVE, &iFlags, sizeof(iFlags)) == -1)  
    {
    	mErr = errno;
        return -1;  
    }
	if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPIDLE, &iIdle, sizeof(iIdle)) == -1)  
    {
    	mErr = errno;
        return -1;  
    }
	if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPINTVL, &iIntvl, sizeof(iIntvl)) == -1)  
    {
    	mErr = errno;
        return -1;  
    }
	if (setsockopt(mFD, IPPROTO_TCP, TCP_KEEPCNT, &iCnt, sizeof(iCnt)) == -1)  
    {
    	mErr = errno;
        return -1;  
    }

    //Linux Kernel 2.6.37
    //如果发送出去的数据包在十秒内未收到ACK确认，则下一次调用send或者recv，则函数会返回-1，errno设置为ETIMEOUT
#ifdef TCP_USER_TIMEOUT
    unsigned int iTimeout = 10000;
	if (setsockopt(mFD, IPPROTO_TCP, TCP_USER_TIMEOUT, &iTimeout, sizeof(iTimeout)) == -1)  
    {
    	mErr = errno;
        return -1;
    }
#endif
    
    return 0;
}
