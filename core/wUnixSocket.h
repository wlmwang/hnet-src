
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_UNIX_SOCKET_H_
#define _W_UNIX_SOCKET_H_

#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include "wSocket.h"

/**
 *  UNIX Domain Socket基础类
 */
class wUnixSocket : public wSocket
{
	public:
		wUnixSocket(SOCK_TYPE eType = SOCK_TYPE_LISTEN, SOCK_PROTO eProto = SOCK_PROTO_UNIX, SOCK_FLAG eFlag = SOCK_FLAG_RVSD) : wSocket(eType, eProto, eFlag) {}

		virtual int Open();
		virtual int Bind(string sHost, unsigned int nPort = 0);	//sHost为sock路径
		virtual int Listen(string sHost, unsigned int nPort = 0);
		virtual int Connect(string sHost, unsigned int nPort = 0, float fTimeout = 30);
		virtual int Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize);	
};

#endif
