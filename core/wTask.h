
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TASK_H_
#define _W_TASK_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"
#include "wDispatch.h"

namespace hnet {

class wSocket;
class wServer;
class wMultiClient;
class wWorkerIpc;

class wTask : private wNoncopyable {
public:
    wTask(wSocket *socket, int32_t type = 0);
    virtual ~wTask();
    
    virtual const char* Name() {
        return "wTask";
    }

    // 同步发送确切长度消息
    // wStatus返回不为空，则task被关闭
    // size = -1 对端发生错误|稍后重试|对端关闭
    // size >= 0 发送字符
    wStatus SyncSend(char cmd[], size_t len, ssize_t *size);
    
    // SyncSend的异步发送版本
    wStatus AsyncSend(char cmd[], size_t len);

    // 同步接受确切长度消息
    // 调用者：保证此sock未加入epoll中，否则出现事件竞争！另外也要确保buf有足够长的空间接受自此同步消息
    // wStatus返回不为空，则socket被关闭
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    wStatus SyncRecv(char cmd[], size_t len, ssize_t *size, uint32_t timeout /*s*/);

    inline wSocket *Socket() {
        return mSocket;
    }
    
    inline size_t SendLen() {
        return mSendLen;
    }
    
    inline int32_t Type() {
        return mType;
    }

protected:
    friend class wServer;
    friend class wMultiClient;
    friend class wWorkerIpc;

    // 登录验证
    virtual wStatus Login() {
        return mStatus = wStatus::Nothing();
    }

    // 处理接受到数据，转发给业务处理函数 Handlemsg 处理。每条消息大小[1b,512k]
    // wStatus返回不为空，则task被关闭；Handlemsg处理出错返回，task也被关闭
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    virtual wStatus TaskRecv(ssize_t *size);

    // 处理接受到数据
    // wStatus返回不为空，则task被关闭
    // size = -1 对端发生错误|稍后重试|对端关闭
    // size >= 0 发送字符
    virtual wStatus TaskSend(ssize_t *size);

    // 解析消息
    // wStatus返回不为空，则task被关闭
    virtual wStatus Handlemsg(char cmd[], uint32_t len);

    // 异步发送：将待发送客户端消息写入buf，等待TaskSend发送
    // wStatus返回不为空，则task被关闭
    wStatus Send2Buf(char cmd[], size_t len);

    wStatus HeartbeatSend();

    inline bool HeartbeatOut() {
        return mHeartbeat > kHeartbeat;
    }

    inline void HeartbeatReset() {
        mHeartbeat = 0;
    }

    // 设置服务端对象（方便异步发送）
    void SetServer(wServer* server) {
    	mSCType = 0;
    	mServer = server;
    }

    // 设置客户端对象（方便异步发送）
    void SetClient(wMultiClient* client) {
    	mSCType = 1;
    	mClient = client;
    }

    int8_t mState;
    wStatus mStatus;

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

protected:
    // 路由规则
    DEC_DISP(mDispatch);
    void RegisterFunc(uint8_t cmd, uint8_t para, TaskDisp func) {
        REG_DISP(mDispatch, Name(), cmd, para, func);
    }
};

}	// namespace hnet

#endif
