
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

// keep-alive 间隔，次数
const uint32_t  kKeepAliveTm = 3000;
const uint8_t   kKeepAliveCnt = 5;

// Tcp Socket基础类
class wTcpSocket : public wSocket {
public:
    wTcpSocket(SockType type = kStListen, SockProto proto = kSpTcp, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag), mIsKeepAlive(true) { }

    virtual wStatus Open();
    virtual wStatus Bind(string host, uint16_t port = 0);
    virtual wStatus Listen(string host, uint16_t port = 0);
    virtual int Connect(string host, uint16_t port = 0, float timeout = 30);
    virtual int Accept(struct sockaddr* clientaddr, socklen_t *addrsize);

    virtual wStatus SetTimeout(float timeout = 30);	//单位：秒
    virtual wStatus SetSendTimeout(float timeout = 30);
    virtual wStatus SetRecvTimeout(float timeout = 30);

protected:
    wStatus SetKeepAlive(int idle = 5, int intvl = 1, int cnt = 10);	// tcp保活

    bool mIsKeepAlive;
};

}	// namespace hnet

#endif
