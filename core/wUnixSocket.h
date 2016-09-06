
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_UNIX_SOCKET_H_
#define _W_UNIX_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wStatus.h"
#include "wSocket.h"

namespace hnet {

// UNIX Domain Socket基础类
class wUnixSocket : public wSocket {
public:
	wUnixSocket(SockType type = kStListen, SockProto proto = kSpUnix, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag) { }

	virtual wStatus Open();
	virtual wStatus Bind(string host, uint16_t port = 0);	//sHost为sock路径
	virtual wStatus Listen(string host, uint16_t port = 0);
	virtual int Connect(string host, uint16_t port = 0, float timeout = 30);
	virtual int Accept(struct sockaddr* clientaddr, socklen_t *addrsize);	
};

}	// namespace hnet

#endif