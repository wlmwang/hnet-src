
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TCP_SOCKET_H_
#define _W_TCP_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wSocket.h"

namespace hnet {

class wTcpSocket : public wSocket {
public:
    wTcpSocket(SockType type = kStListen, SockProto proto = kSpTcp, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag), mIsKeepAlive(true) { }

    virtual int Accept(int* fd, struct sockaddr* clientaddr, socklen_t *addrsize);
    virtual int Connect(const std::string& host, uint16_t port = 0, float timeout = 30);
    virtual int Listen(const std::string& host, uint16_t port = 0);
    virtual int Open();

    virtual int SetTimeout(float timeout = 30);
    virtual int SetSendTimeout(float timeout = 30);
    virtual int SetRecvTimeout(float timeout = 30);

protected:
    virtual int Bind(const std::string& host, uint16_t port = 0);
    int SetKeepAlive(int idle = 5, int intvl = 1, int cnt = 10);	// tcp保活

    bool mIsKeepAlive;
};

}	// namespace hnet

#endif
