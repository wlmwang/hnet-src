
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_UDP_SOCKET_H_
#define _W_UDP_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wStatus.h"
#include "wSocket.h"

namespace hnet {

// Udp Socket基础类
class wUdpSocket : public wSocket {
public:
	wUdpSocket(SockType type = kStConnect, SockProto proto = kSpUdp, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag) { }

	virtual const wStatus& RecvBytes(char buf[], size_t len, ssize_t *size);
    virtual const wStatus& SendBytes(char buf[], size_t len, ssize_t *size);

    virtual const wStatus& Open();
    virtual const wStatus& Listen(const std::string& host, uint16_t port = 0);

protected:
    virtual const wStatus& Bind(const std::string& host, uint16_t port = 0);

    std::string mClientHost;
    uint16_t mClientPort;
};

}	// namespace hnet

#endif
