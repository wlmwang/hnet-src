
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_UNIX_SOCKET_H_
#define _W_UNIX_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wSocket.h"

namespace hnet {

const char kUnixSockPrefix[] = "/tmp/hnet_";

class wUnixSocket : public wSocket {
public:
	wUnixSocket(SockType type = kStListen, SockProto proto = kSpUnix, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag) { }
	virtual ~wUnixSocket();
	
	virtual int Accept(int* fd, struct sockaddr* clientaddr, socklen_t *addrsize);
	virtual int Connect(const std::string& host, uint16_t port = 0, float timeout = 30);
	virtual int Listen(const std::string& host, uint16_t port = 0);	  // host为sock路径
	virtual int Open();

    virtual int SetTimeout(float timeout = 30);
    virtual int SetSendTimeout(float timeout = 30);
    virtual int SetRecvTimeout(float timeout = 30);

protected:
	virtual int Bind(const std::string& host, uint16_t port = 0);	// host为sock路径
};

}	// namespace hnet

#endif
