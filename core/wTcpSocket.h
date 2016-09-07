
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TCP_SOCKET_H_
#define _W_TCP_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wStatus.h"
#include "wSocket.h"

namespace hnet {

// Tcp Socket基础类
class wTcpSocket : public wSocket {
public:
    wTcpSocket(SockType type = kStListen, SockProto proto = kSpTcp, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag), mIsKeepAlive(true) { }

    virtual wStatus Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize);
    virtual wStatus Connect(int64_t *ret, string host, uint16_t port = 0, float timeout = 30);
    virtual wStatus Bind(string host, uint16_t port = 0);
    virtual wStatus Listen(string host, uint16_t port = 0);
    virtual wStatus Open();

    virtual wStatus SetTimeout(float timeout = 30);
    virtual wStatus SetSendTimeout(float timeout = 30);
    virtual wStatus SetRecvTimeout(float timeout = 30);

protected:
    wStatus SetKeepAlive(int idle = 5, int intvl = 1, int cnt = 10);	// tcp保活

    bool mIsKeepAlive;
};

}	// namespace hnet

#endif
