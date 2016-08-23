
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wUnixSocket.h"

int wUnixSocket::Open()
{
	if ((mFD = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		mErr = errno;
		return -1;
	}
	return mFD;
}

int wUnixSocket::Bind(string sHost, unsigned int nPort)
{
	if (mFD == FD_UNKNOWN)
	{
		return -1;
	}
	struct sockaddr_un stSocketAddr;
	memset(&stSocketAddr, 0, sizeof(stSocketAddr));
	stSocketAddr.sun_family = AF_UNIX;
	strncpy(stSocketAddr.sun_path, sHost.c_str(), sizeof(stSocketAddr.sun_path) - 1);

	if (bind(mFD, (struct sockaddr *)&stSocketAddr, sizeof(stSocketAddr)) < 0)
	{
		mErr = errno;
		Close();
		return -1;
	}
	return 0;
}

int wUnixSocket::Listen(string sHost, unsigned int nPort)
{
	if (mFD == FD_UNKNOWN)
	{
		return -1;
	}
	mHost = sHost;

	if (Bind(mHost) < 0)
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

int wUnixSocket::Connect(string sHost, unsigned int nPort, float fTimeout)
{
	if (mFD == FD_UNKNOWN)
	{
		return -1;
	}
	mHost = sHost;

	string sTmpSock = "unix_";
	sTmpSock += Itos(getpid()) + ".sock";
	if (Bind(sTmpSock) < 0)
	{
		mErr = errno;
		Close();
		return -1;
	}

	struct sockaddr_un stSockAddr;
	memset(&stSockAddr, 0, sizeof(stSockAddr));
	stSockAddr.sun_family = AF_UNIX;
	strncpy(stSockAddr.sun_path, mHost.c_str(), sizeof(stSockAddr.sun_path) - 1);

	//超时设置
	if (fTimeout > 0)
	{
		SetNonBlock();
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
                    LOG_ERROR(ELOG_KEY, "[system] unix connect timeout millisecond=%d", iTimeout);
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
                        LOG_ERROR(ELOG_KEY, "[system] ip=%s:%d, unix connect getsockopt errno=%d,%s", mHost.c_str(), mPort, mErr, strerror(mErr));
                        return -1;
                    }
                    if(iVal > 0)
                    {
                    	mErr = errno;
                        LOG_ERROR(ELOG_KEY, "[system] ip=%s:%d, unix connect fail errno=%d,%s", mHost.c_str(), mPort, mErr, strerror(mErr));
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
            LOG_ERROR(ELOG_KEY, "[system] ip=%s:%d, unix connect directly errno=%d,%s", mHost.c_str(), mPort, mErr, strerror(mErr));
			Close();
			return -1;
		}
	}
	return 0;
}

int wUnixSocket::Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize)
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

	return iNewFD;
}
