
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SOCKET_H_
#define _W_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

enum SockErr {
    kSeNobuff = -1, // socket buf写满
    kSeTimeout = -2,// connect连接超时
    kSeMsgLen = -3, // 接受消息长度不合法
    kSeClosed = -4  // socket对端close
};

enum SockType   {
    kStUnknown = 0,
    kStListen,      // 监听socket
    kStConnect      // 连接socket
};

enum SockStatus {
    kSsUnknown = 0,
    kSsListened,    // 正监听
    kSsUnconnect,   // 未连接
    kSsConnected    // 已连接
};

enum SockProto  { kSpUnknown = 0, kSpTcp, kSpUdp, kSpUnix, kSpChannel, kSpHttp};
enum SockFlag   { kSfUnknown = 0, kSfRvsd, kSfRecv, kSfSend};

class wSocket : private wNoncopyable {
public:
    wSocket(SockType type = kStListen, SockProto proto = kSpTcp, SockFlag flag = kSfRvsd);
    virtual ~wSocket();

    // 从客户端接收数据
    // wStatus返回不为空，则socket被关闭
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    virtual const wStatus& RecvBytes(char buf[], size_t len, ssize_t *size);

    // 发送到客户端数据
    // wStatus返回不为空，则socket被关闭
    // size = -1 对端发生错误|稍后重试|对端关闭
    // size >= 0 发送字符
    virtual const wStatus& SendBytes(char buf[], size_t len, ssize_t *size);
    
    // 从客户端接收连接
    // fd = -1 发生错误|稍后重试
    // fd > 0 新文件描述符
    virtual const wStatus& Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
        return mStatus = wStatus::IOError("wSocket::Accept failed", "method should be inherit");
    }
    
    // 连接服务器
    // ret = -1 发生错误
    // ret = 0 连接成功
    virtual const wStatus& Connect(int64_t *ret, const std::string& host, uint16_t port = 0, float timeout = 30) {
        return mStatus = wStatus::IOError("wSocket::Connect failed", "method should be inherit");
    }
    
    virtual const wStatus& Listen(const std::string& host, uint16_t port = 0) {
        return mStatus = wStatus::IOError("wSocket::Listen failed", "method should be inherit");
    }
    
    virtual const wStatus& Open() {
        return mStatus = wStatus::IOError("wSocket::Open failed", "method should be inherit");
    }

    virtual const wStatus& SetTimeout(float fTimeout = 30) {
        return mStatus = wStatus::IOError("wSocket::SetTimeout failed", "method should be inherit");
    }

    virtual const wStatus& SetSendTimeout(float fTimeout = 30) {
        return mStatus = wStatus::IOError("wSocket::SetSendTimeout failed", "method should be inherit");
    }

    virtual const wStatus& SetRecvTimeout(float fTimeout = 30) {
        return mStatus = wStatus::IOError("wSocket::SetRecvTimeout failed", "method should be inherit");
    }

    virtual const wStatus& Close();
    virtual const wStatus& SetFL(bool nonblock = true);
    
    inline int64_t& FD() { return mFD;}
    inline std::string& Host() { return mHost;}
    inline uint16_t& Port() { return mPort;}
    inline uint64_t& RecvTm() { return mRecvTm;}
    inline uint64_t& SendTm() { return mSendTm;}
    inline uint64_t& MakeTm() { return mMakeTm;}
		
    //socket状态属性
    inline SockType& ST() { return mSockType;}
    inline SockStatus& SS() { return mSockStatus;}
    inline SockProto& SP() { return mSockProto;}
    inline SockFlag& SF() { return mSockFlag;}
    
protected:
    virtual const wStatus& Bind(const std::string& host, uint16_t port = 0) = 0;
    
    int64_t  mFD;
    std::string mHost;
    uint16_t mPort;
    uint64_t mRecvTm;   // 最后接收到数据包的时间戳
    uint64_t mSendTm;   // 最后发送数据包时间戳（主要用户心跳检测）
    uint64_t mMakeTm;   // 创建时间

    SockType mSockType;
    SockStatus mSockStatus;
    SockProto mSockProto;
    SockFlag mSockFlag;

    wStatus mStatus;
};

}   // namespace hnet

#endif
