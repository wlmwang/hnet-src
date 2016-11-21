
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TASK_H_
#define _W_TASK_H_

#include <google/protobuf/message.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wEvent.h"

namespace hnet {

// 消息绑定函数参数类型
struct Request_t {
	char* mBuf;
	uint32_t mLen;
	Request_t(char buf[], uint32_t len) : mBuf(buf), mLen(len) { }
};

class wSocket;
class wServer;
class wMultiClient;

class wTask : private wNoncopyable {
public:
    wTask(wSocket *socket, int32_t type = 0);
    void ResetBuffer();
    virtual ~wTask();

    // 登录验证
    virtual const wStatus& Login() {
        return mStatus.Clear();
    }

    // 处理接受到数据，转发给业务处理函数 Handlemsg 处理。每条消息大小[1b,512k]
    // wStatus返回不为空，则task被关闭；Handlemsg处理出错返回，task也被关闭
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    virtual const wStatus& TaskRecv(ssize_t *size);

    // 处理接受到数据
    // wStatus返回不为空，则task被关闭
    // size = -1 对端发生错误|稍后重试|对端关闭
    // size >= 0 发送字符
    virtual const wStatus& TaskSend(ssize_t *size);

    // 解析消息
    // wStatus返回不为空，则task被关闭
    virtual const wStatus& Handlemsg(char cmd[], uint32_t len);

    // 异步发送：将待发送客户端消息写入buf，等待TaskSend发送
    // wStatus返回不为空，则task被关闭
    const wStatus& Send2Buf(char cmd[], size_t len);
    const wStatus& Send2Buf(const google::protobuf::Message* msg);

    // 同步发送确切长度消息
    // wStatus返回不为空，则task被关闭
    // size = -1 对端发生错误|稍后重试|对端关闭
    // size >= 0 发送字符
    const wStatus& SyncSend(char cmd[], size_t len, ssize_t *size);
    const wStatus& SyncSend(const google::protobuf::Message* msg, ssize_t *size);

    // SyncSend的异步发送版本
    const wStatus& AsyncSend(char cmd[], size_t len);
    const wStatus& AsyncSend(const google::protobuf::Message* msg);

    // 同步接受一条合法的、非心跳消息
    // 调用者：保证此sock未加入epoll中，否则出现事件竞争！另外也要确保buf有足够长的空间接受自此同步消息
    // wStatus返回不为空，则socket被关闭
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    const wStatus& SyncRecv(char cmd[], ssize_t *size, uint32_t timeout = 30);
    const wStatus& SyncRecv(google::protobuf::Message* msg, ssize_t *size, uint32_t timeout = 30);

    // 广播其他worker进程
    const wStatus& SyncWorker(char cmd[], size_t len);
    const wStatus& SyncWorker(const google::protobuf::Message* msg);

    static void Assertbuf(char buf[], const char cmd[], size_t len);
    static void Assertbuf(char buf[], const google::protobuf::Message* msg);

    const wStatus& HeartbeatSend();

    inline bool HeartbeatOut() {
        return mHeartbeat > kHeartbeat;
    }

    inline void HeartbeatReset() {
        mHeartbeat = 0;
    }

    // 设置服务端对象（方便异步发送）
    inline void SetServer(wServer* server) {
    	mSCType = 0;
    	mServer = server;
    }

    // 设置客户端对象（方便异步发送）
    inline void SetClient(wMultiClient* client) {
    	mSCType = 1;
    	mClient = client;
    }

    inline wSocket *Socket() { return mSocket;}
    
    inline size_t SendLen() { return mSendLen;}
    
    inline int32_t Type() { return mType;}

protected:
    // command消息路由器
    template<typename T = wTask>
    void On(int8_t cmd, int8_t para, int (T::*func)(struct Request_t *argv), T* target) {
    	mEventCmd.On(CmdId(cmd, para), std::bind(func, target, std::placeholders::_1));
    }
    wEvent<uint16_t, std::function<int(struct Request_t *argv)>, struct Request_t*> mEventCmd;

    // protobuf消息路由器
    template<typename T = wTask>
    void On(const std::string& pbname, int (T::*func)(struct Request_t *argv), T* target) {
    	mEventPb.On(pbname, std::bind(func, target, std::placeholders::_1));
    }
    wEvent<std::string, std::function<int(struct Request_t *argv)>, struct Request_t*> mEventPb;

    int32_t mType;
    wSocket *mSocket;
    uint8_t mHeartbeat;

    char mTempBuff[kPackageSize];    // 同步发送、接受消息缓冲
    char mRecvBuff[kPackageSize];    // 异步接受消息缓冲
    char mSendBuff[kPackageSize];    // 异步发送消息缓冲
    
    char *mRecvRead;
    char *mRecvWrite;
    size_t mRecvLen;  // 已接受数据长度

    char *mSendRead;
    char *mSendWrite;
    size_t mSendLen;  // 可发送数据长度

    wServer* mServer;
    wMultiClient* mClient;

    // 0为服务器，1为客户端
    uint8_t mSCType;

    wStatus mStatus;
};

}	// namespace hnet

#endif
