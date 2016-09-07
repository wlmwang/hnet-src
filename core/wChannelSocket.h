
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CHANNEL_SOCKET_H_
#define _W_CHANNEL_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wStatus.h"
#include "wSocket.h"

namespace hnet {

// socketpair基础类
// master-worker间的IPC通信
class wChannelSocket : public wSocket {
public:
	wChannelSocket(SockType type = kStListen, SockProto proto = kSpChannel, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag) { }

	virtual wStatus Open();
	
	virtual wStatus RecvBytes(char buf[], size_t len, ssize_t *size);
	
	virtual wStatus SendBytes(char buf[], size_t len, ssize_t *size);
	
	virtual void Close();

	int &operator[](int i);
protected:
	int mCh[2] {FD_UNKNOWN, FD_UNKNOWN};
};

}	// namespace hnet


#endif
