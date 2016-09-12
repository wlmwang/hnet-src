
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
    wChannelSocket(SockType type = kStListen, SockProto proto = kSpChannel, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag) {
        mChannel[0] = mChannel[1] = kFDUnknown;
    }
    
    virtual ~wChannelSocket();
    
    virtual wStatus Open();

    virtual wStatus RecvBytes(char buf[], size_t len, ssize_t *size);

    virtual wStatus SendBytes(char buf[], size_t len, ssize_t *size);

    virtual wStatus Close();

    int64_t &operator[](uint8_t i);

protected:
    // 0:传递给其他进程，供写入数据 
    // 1:当前进程读去其他进程写入0中的数据
    int64_t mChannel[2];
};

}   // namespace hnet

#endif