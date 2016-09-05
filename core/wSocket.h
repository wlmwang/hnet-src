
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SOCKET_H_
#define _W_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wLog.h"
#include "wMisc.h"
#include "wNoncopyable.h"

namespace hnet {

enum SockType { kSockTypeUnknown = 0, kSockTypeListen, kSockTypeConnect };
enum SockStatus { kSockStatusUnknown = 0, kSockStatusUnconnect, kSockStatusConnected, kSockStatusListened };
enum SockFlag { kSockFlagUnknown = 0, kSockFlagRvsd, kSockFlagRecv, kSockFlagSend };
enum SockProto { kSockProtoUnknown = 0, kSockProtoTcp, kSockProtoUdp, kSockProtoUnix, kSockProtoChannel, kSockProtoHttp};

class wSocket : private wNoncopyable {
public:
    wSocket(SOCK_TYPE eType = SOCK_TYPE_LISTEN, SOCK_PROTO eProto = SOCK_PROTO_TCP, SOCK_FLAG eFlag = SOCK_FLAG_RVSD);
    virtual ~wSocket();

    // 从客户端接收数据
    // return ：<0 对端发生错误|消息超长|对端关闭(FIN_WAIT) =0 稍后重试 >0 接受字符
    virtual ssize_t RecvBytes(char *vArray, size_t vLen);

    // 发送客户端数据
    // return ：<0 对端发生错误 >=0 发送字符
    virtual ssize_t SendBytes(char *vArray, size_t vLen);
    
    // 从客户端接收连接
    // return ：<0 对端发生错误|对端关闭(FIN_WAIT) =0 稍后重试 >0 文件描述符
    virtual int Accept(struct sockaddr* pClientSockAddr, socklen_t *pSockAddrSize) { return -1;}
    virtual int Bind(string sHost, unsigned int nPort = 0) { return -1;}
    virtual int Listen(string sHost, unsigned int nPort = 0) { return -1;}
    virtual int Connect(string sHost, unsigned int nPort = 0, float fTimeout = 30) { return -1;}
    virtual int Open() { return FD_UNKNOWN;}

    virtual void Close();
    virtual int SetNonBlock(bool bNonblock = true);

    virtual int SetTimeout(float fTimeout = 30) { return -1; }
    virtual int SetSendTimeout(float fTimeout = 30) { return -1; } 
    virtual int SetRecvTimeout(float fTimeout = 30) { return -1; }

    int &Errno() { return mErr; }
    int &FD() { return mFD; }

    SOCK_TYPE &SockType() { return mSockType;}
    SOCK_STATUS &SockStatus() { return mSockStatus;}
    SOCK_PROTO &SockProto() { return mSockProto;}
    SOCK_FLAG &SockFlag() { return mSockFlag; }

    unsigned long long &RecvTime() { return mRecvTime; }
    unsigned long long &SendTime() { return mSendTime; }
    unsigned long long &CreateTime() { return mCreateTime; }

    virtual string &Host() { return mHost; }
    virtual unsigned short &Port() { return mPort; }

protected:
    int mErr;
    string mHost;
    unsigned short mPort {0};

    int mFD {FD_UNKNOWN};
    SOCK_TYPE mSockType {SOCK_TYPE_UNKNOWN};
    SOCK_STATUS mSockStatus {SOCK_STATUS_UNKNOWN};
    SOCK_PROTO mSockProto {SOCK_PROTO_UNKNOWN};
    SOCK_FLAG mSockFlag {SOCK_FLAG_UNKNOWN};

    unsigned long long mRecvTime {0};	//最后接收到数据包的时间戳，毫秒
    unsigned long long mSendTime {0};	//最后发送数据包时间戳（主要用户心跳检测），毫秒
    unsigned long long mCreateTime {0};	//创建时间，毫秒
};

}   // namespace hnet

#endif
