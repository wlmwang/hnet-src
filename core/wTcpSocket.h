
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TCP_SOCKET_H_
#define _W_TCP_SOCKET_H_

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>

#include "wSocket.h"

/**
 *  Tcp Socket基础类
 */
class wTcpSocket : public wSocket
{
	public:
		wTcpSocket(SOCK_TYPE eType = SOCK_TYPE_LISTEN, SOCK_PROTO eProto = SOCK_PROTO_TCP, SOCK_FLAG eFlag = SOCK_FLAG_RVSD) : wSocket(eType, eProto, eFlag) {}
		
		virtual int Open();
		virtual int Bind(string sHost, unsigned int nPort = 0);
		virtual int Listen(string sHost, unsigned int nPort = 0);
		virtual int Connect(string sHost, unsigned int nPort = 0, float fTimeout = 30);
		virtual int Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize);
		
		virtual int SetTimeout(float fTimeout = 30);	//单位：秒
		virtual int SetSendTimeout(float fTimeout = 30);
		virtual int SetRecvTimeout(float fTimeout = 30);
		
		int SetKeepAlive(int iIdle = 5, int iIntvl = 1, int iCnt = 10);	//tcp保活
	protected:
		bool mIsKeepAlive {true};
};

#endif
