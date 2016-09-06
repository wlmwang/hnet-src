
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
    // return ：<0 对端发生错误|消息超长|对端关闭(FIN_WAIT) =0 稍后重试 >0 接受字符
    virtual ssize_t RecvBytes(char *vArray, size_t vLen);

    // 发送客户端数据
    // return ：<0 对端发生错误 >=0 发送字符
    virtual ssize_t SendBytes(char *vArray, size_t vLen);
    
    // 从客户端接收连接
    // return ：<0 对端发生错误|对端关闭(FIN_WAIT) =0 稍后重试 >0 文件描述符
    virtual int Accept(struct sockaddr* clientaddr, socklen_t *addrsize) { 
	return -1;
    }
    
    virtual int Connect(string host, uint16_t port = 0, float timeout = 30) { 
	return -1;
    }
    
    virtual wStatus Bind(string host, uint16_t port = 0) { 
	return wStatus::Nothing();
    }
    
    virtual wStatus Listen(string host, uint16_t port = 0) { 
	return wStatus::Nothing();
    }
    
    virtual wStatus Open() { 
	return wStatus::Nothing();
    }
    
    virtual wStatus Close();
    virtual wStatus SetFL(bool nonblock = true);


    virtual wStatus SetTimeout(float fTimeout = 30) { return -1; }
    virtual wStatus SetSendTimeout(float fTimeout = 30) { return -1; } 
    virtual wStatus SetRecvTimeout(float fTimeout = 30) { return -1; }

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
    
    int mFD;
    uint16_t mPort;
    string mHost;

    SockType mSockType;
    SockStatus mSockStatus;
    SockProto mSockProto;
    SockFlag mSockFlag;

    uint64_t mRecvTm;   // 最后接收到数据包的时间戳
    uint64_t mSendTm;   // 最后发送数据包时间戳（主要用户心跳检测）
    uint64_t mMakeTm;   // 创建时间
};

}   // namespace hnet

#endif
