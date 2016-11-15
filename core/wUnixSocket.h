
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
	virtual wStatus Listen(const std::string& host, uint16_t port = 0);	  // host为sock路径
	virtual wStatus Connect(int64_t *fd, const std::string& host, uint16_t port = 0, float timeout = 30);
	virtual wStatus Accept(int64_t *ret, struct sockaddr* clientaddr, socklen_t *addrsize);
	
protected:
	virtual wStatus Bind(const std::string& host, uint16_t port = 0);	// host为sock路径
};

}	// namespace hnet

#endif
