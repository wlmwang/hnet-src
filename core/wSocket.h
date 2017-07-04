
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SOCKET_H_
#define _W_SOCKET_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wMisc.h"
#include "wNoncopyable.h"
#include "wLogger.h"

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

    // 设置、获取描述符阻塞状态
    // 返回 =-1 操作失败
    virtual int SetNonblock(bool nonblock = true);
    virtual int GetNonblock();

    // 关闭一个描述符
    virtual int Close();

    // 从客户端接收数据
    // size =-1 对端发生错误|稍后重试
    // size = 0 对端关闭
    // size > 0 接受字符
    // 返回 =-1 表示需要关闭该连接，并清理内存
    virtual int RecvBytes(char buf[], size_t len, ssize_t *size);

    // 发送到客户端数据
    // size =-1 对端发生错误|稍后重试|对端关闭
    // size>= 0 发送字符
    // 返回 =-1 表示需要关闭该连接，并清理内存
    virtual int SendBytes(char buf[], size_t len, ssize_t *size);
    
    // 从客户端接收连接
    // fd   =-1 发生错误|稍后重试
    // fd   > 0 新描述符值
    // 返回 =-1 表示需要关闭该连接，并清理内存
    virtual int Accept(int* fd, struct sockaddr* clientaddr, socklen_t *addrsize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::Accept () failed", "method should be inherit");
        return -1;
    }

    // 客户端连接服务器
    // 返回 =-1 表示需要关闭该连接，并清理内存
    virtual int Connect(const std::string& host, uint16_t port = 0, float timeout = 30) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::Connect () failed", "method should be inherit");
        return -1;
    }
    
    // 监听socket描述符
    // 返回 =-1 表示需要关闭该连接，并清理内存
    virtual int Listen(const std::string& host, uint16_t port = 0) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::Listen () failed", "method should be inherit");
        return -1;
    }
    
    // 创建socket描述符
    // 返回 =-1 表示需要关闭该连接，并清理内存
    virtual int Open() {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::Open () failed", "method should be inherit");
        return -1;
    }

    // 设置描述符接受、发送超时时间
    // 返回 =-1 设置失败
    virtual int SetTimeout(float fTimeout = 30) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::SetTimeout () failed", "method should be inherit");
        return -1;
    }

    // 设置描述符发送超时时间
    // 返回 =-1 设置失败
    virtual int SetSendTimeout(float fTimeout = 30) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::SetSendTimeout () failed", "method should be inherit");
        return -1;
    }

    // 设置描述符接受超时时间
    // 返回 =-1 设置失败
    virtual int SetRecvTimeout(float fTimeout = 30) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::SetRecvTimeout () failed", "method should be inherit");
        return -1;
    }

    inline std::string& Host() { return mHost;}
    inline const std::string& Host() const { return mHost;}
    inline uint16_t& Port() { return mPort;}
    inline const uint16_t& Port() const { return mPort;}

    inline int64_t& FD() { return mFD;}
    inline uint64_t& RecvTm() { return mRecvTm;}
    inline uint64_t& SendTm() { return mSendTm;}
    inline uint64_t& MakeTm() { return mMakeTm;}
		
    // socket描述符状态属性
    inline SockType& ST() { return mSockType;}
    inline SockStatus& SS() { return mSockStatus;}
    inline SockProto& SP() { return mSockProto;}
    inline SockFlag& SF() { return mSockFlag;}
    
    inline bool operator==(const wSocket& rval) {
        return mFD == rval.mFD && mSockType == rval.mSockType && mSockProto == rval.mSockProto;
    }

protected:
    // socket描述符绑定host、port
    // 返回 =-1 绑定失败
    virtual int Bind(const std::string& host, uint16_t port = 0) = 0;

protected:
    int64_t  mFD;

    std::string mHost;
    uint16_t    mPort;

    uint64_t    mRecvTm;   // 最后接收到数据包的时间戳
    uint64_t    mSendTm;   // 最后发送数据包时间戳
    uint64_t    mMakeTm;   // 创建时间

    SockType    mSockType;
    SockStatus  mSockStatus;
    SockProto   mSockProto;
    SockFlag    mSockFlag;
};

}   // namespace hnet

#endif
