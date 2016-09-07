
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
    // ret <0 对端发生错误|消息超长|对端关闭(FIN_WAIT) =0 稍后重试 >0 接受字符
    virtual wStatus RecvBytes(char buf[], size_t len, ssize_t *size);

    // 发送到客户端数据
    // return ：<0 对端发生错误 >=0 发送字符
    virtual wStatus SendBytes(char buf[], size_t len, ssize_t *size);
    
    // 从客户端接收连接
    // ret <0 对端发生错误|对端关闭(FIN_WAIT) =0 稍后重试 >0 文件描述符
    virtual wStatus Accept(int64_t *fd, struct sockaddr* clientaddr, socklen_t *addrsize) = 0;
    
    // 连接服务器
    // ret <0 对端发生错误|对端关闭(FIN_WAIT) =0 稍后重试 >0 文件描述符
    virtual wStatus Connect(int64_t *ret, string host, uint16_t port = 0, float timeout = 30) = 0;
    
    virtual wStatus Open() = 0;
    virtual wStatus Bind(string host, uint16_t port = 0) = 0;
    virtual wStatus Listen(string host, uint16_t port = 0) = 0;

    virtual wStatus SetFL(bool nonblock = true);
    virtual wStatus Close();

    virtual wStatus SetTimeout(float fTimeout = 30) {
        return mStatus = wStatus::IOError("wSocket::SetTimeout failed", "method should be inherit");
    }

    virtual wStatus SetSendTimeout(float fTimeout = 30) {
        return mStatus = wStatus::IOError("wSocket::SetSendTimeout failed", "method should be inherit");
    }

    virtual wStatus SetRecvTimeout(float fTimeout = 30) {
        return mStatus = wStatus::IOError("wSocket::SetRecvTimeout failed", "method should be inherit");
    }

    /*
    int &Errno() { return mErr; }
    int &FD() { return mFD; }

    SOCK_TYPE &SockType() { return mSockType;}
    SOCK_STATUS &SockStatus() { return mSockStatus;}
    SOCK_PROTO &SockProto() { return mSockProto;}
    SOCK_FLAG &SockFlag() { return mSockFlag; }
    */
   
    /*
    unsigned long long &RecvTime() { return mRecvTime; }
    unsigned long long &SendTime() { return mSendTime; }
    unsigned long long &CreateTime() { return mCreateTime; }

    virtual string &Host() { return mHost; }
    virtual unsigned short &Port() { return mPort; }
    */
   
protected:
    wStatus mStatus;
    
    SockType mSockType;
    SockStatus mSockStatus;
    SockProto mSockProto;
    SockFlag mSockFlag;

    int mFD;
    uint16_t mPort;
    string mHost;

    uint64_t mRecvTm;   // 最后接收到数据包的时间戳
    uint64_t mSendTm;   // 最后发送数据包时间戳（主要用户心跳检测）
    uint64_t mMakeTm;   // 创建时间
};

}   // namespace hnet

#endif
