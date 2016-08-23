
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSocket.h"

wSocket::wSocket(SOCK_TYPE eType, SOCK_PROTO eProto, SOCK_FLAG eFlag) : mSockType(eType), mSockProto(eProto), mSockFlag(eFlag)
{
	mCreateTime = GetTickCount();
}

wSocket::~wSocket()
{
	Close();
}

void wSocket::Close()
{
	if(mFD != FD_UNKNOWN) 
	{
		close(mFD);
	}
	mFD = FD_UNKNOWN;
}

int wSocket::SetNonBlock(bool bNonblock)
{
	if(mFD == FD_UNKNOWN) 
	{
		return -1;
	}
	
	int iFlags = fcntl(mFD, F_GETFL, 0);
	return fcntl(mFD, F_SETFL, (bNonblock == true ? iFlags | O_NONBLOCK : iFlags & ~O_NONBLOCK));
}

ssize_t wSocket::RecvBytes(char *vArray, size_t vLen)
{
	mRecvTime = GetTickCount();
	
	ssize_t iRecvLen;
	while (true)
	{
		iRecvLen = recv(mFD, vArray, vLen, 0);
		if (iRecvLen > 0)
		{
			return iRecvLen;
		}
		else if (iRecvLen == 0)	//关闭
		{
			return ERR_CLOSED;	//FIN
		}
		else
		{
			mErr = errno;
			if (mErr == EINTR)	//中断
			{
				continue;
			}
			if (mErr == EAGAIN || mErr == EWOULDBLOCK)	//暂时无数据可读，可以继续读，或者等待epoll的后续通知
			{
				return 0;
			}
			
			LOG_ERROR(ELOG_KEY, "[system] recv fd(%d) error: %s", mFD, strerror(mErr));
			return iRecvLen;
		}
	}
}

ssize_t wSocket::SendBytes(char *vArray, size_t vLen)
{
	mSendTime = GetTickCount();
	
	ssize_t iSendLen;
	size_t iLeftLen = vLen;
	size_t iHaveSendLen = 0;
	while (true)
	{
		iSendLen = send(mFD, vArray + iHaveSendLen, iLeftLen, 0);
		if (iSendLen >= 0)
		{
			iLeftLen -= iSendLen;
			iHaveSendLen += iSendLen;
			if(iLeftLen == 0)
			{
				return vLen;
			}
		}
		else
		{
			mErr = errno;
			if (mErr == EINTR) //中断
			{
				continue;
			}
			if (iSendLen < 0 && (mErr == EAGAIN || mErr == EWOULDBLOCK))	//当前缓冲区写满，可以继续写，或者等待epoll的后续通知
			{
				return 0;
			}
			
			LOG_ERROR(ELOG_KEY, "[system] send fd(%d) error: %s", mFD, strerror(mErr));
			return iSendLen;
		}
	}
}
