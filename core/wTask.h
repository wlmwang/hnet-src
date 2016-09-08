
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

class wTask : private wNoncopyable {
public:
    //wTask();
    wTask(wSocket *socket);
    virtual ~wTask();
    
    virtual const char* Name() const = 0;
    
    // 验证接收到登录请求
    virtual wStatus Verify() {
        return mStatus = wStatus::Nothing();
    }

    // 发送登录验证请求
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
    virtual wStatus Handlemsg(char *buf[], uint32_t len) {
        return mStatus = wStatus::IOError("wTask::Handlemsg, socket will be closed", "method should be inherit");
    }
    
    // 异步发送：将待发送客户端消息写入buf，等待TaskSend发送
    // wStatus返回不为空，则task被关闭
    wStatus Send2Buf(const char buf[], size_t len);

    // 同步发送确切长度消息
    // wStatus返回不为空，则task被关闭
    // size = -1 对端发生错误|稍后重试|对端关闭
    // size >= 0 发送字符
    wStatus SyncSend(const char buf[], size_t len, ssize_t *size);
    
    // 同步接受确切长度消息
    // 调用者：保证此sock未加入epoll中，否则出现事件竞争！另外也要确保buf有足够长的空间接受自此同步消息
    // wStatus返回不为空，则socket被关闭
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    wStatus SyncRecv(char buf[], size_t len, size_t *size, uint32_t timeout /*s*/);
   
    wStatus HeartbeatSend();

    void HeartbeatReset() {
        return mHeartbeat = 0;
    }

    bool HeartbeatOuttimes() {
        return mHeartbeat > kHeartbeat;
    }
    
    wSocket *Socket() {
        return mSocket;
    }
    
    size_t SendLen() {
        return mSendLen;
    }
    
    /*
    bool IsRunning() {
        return mStatus == TASK_RUNNING;
    }
    */
protected:
    int8_t mState;
    wStatus mStatus;
    wSocket *mSocket;
    uint8_t mHeartbeat;

    char mTempBuff[kPackageSize];    // 同步发送、接受消息缓冲
    char mRecvBuff[kPackageSize];    // 异步接受消息缓冲
    char mSendBuff[kPackageSize];    // 异步发送消息缓冲
    
    char *mRecvWrite;
    char *mRecvRead;
    size_t  mRecvLen;  // 已接受数据长度

    char *mSendWrite;
    char *mSendRead;
    size_t  mSendLen;  // 可发送数据长度

protected:
    // 路由规则
    DEC_DISP(mDispatch);
    void RegisterFunc(uint8_t cmd, uint8_t para, DispatchFunc func) {
	REG_DISP(mDispatch, Name(), CMD_CHANNEL_REQ, CHANNEL_REQ_OPEN, func);
    }
};

}	// namespace hnet

#endif