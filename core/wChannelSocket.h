
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CHANNEL_SOCKET_H_
#define _W_CHANNEL_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wSocket.h"

namespace hnet {

class wChannelSocket : public wSocket {
public:
    wChannelSocket(SockType type = kStConnect, SockProto proto = kSpChannel, SockFlag flag = kSfRvsd) : wSocket(type, proto, flag) {
        mChannel[0] = mChannel[1] = kFDUnknown;
    }
    
    virtual ~wChannelSocket();
    virtual int Open();
    virtual int Close();
    virtual int RecvBytes(char buf[], size_t len, ssize_t *size);
    virtual int SendBytes(char buf[], size_t len, ssize_t *size);

    inline int& operator[](uint8_t i) {
        assert(i < 2);
        return mChannel[i];
    }

protected:
    virtual int Bind(const std::string& host, uint16_t port = 0) {
        return 0;
    }

    // 0:传递给其他进程，供写入数据
    // 1:当前进程读取其他进程写入0中的数据
    int mChannel[2];
};

}   // namespace hnet

#endif
