
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
	virtual wStatus Bind(string sHost, unsigned int nPort = 0);
	virtual wStatus Listen(string sHost, unsigned int nPort = 0);
	virtual wStatus Connect(string sHost, unsigned int nPort = 0, float timeout = 30);
	virtual wStatus Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize);
	
	virtual wStatus SetTimeout(float timeout = 30);	//单位：秒
	virtual wStatus SetSendTimeout(float timeout = 30);
	virtual wStatus SetRecvTimeout(float timeout = 30);

protected:
	wStatus SetKeepAlive(int iIdle = 5, int iIntvl = 1, int iCnt = 10);	//tcp保活

	bool mIsKeepAlive;
};

}	// namespace hnet

#endif
