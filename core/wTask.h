
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TASK_H_
#define _W_TASK_H_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wSocket;

class wTask : private wNoncopyable {
public:
    wTask();
    wTask(wSocket *pSocket);
    virtual ~wTask();
    
    // 验证接收到登录请求
    virtual int Verify() {
        return 0;
    }

    // 发送登录验证请求
    virtual int Login() {
        return 0;
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
    
    // 解析消息，当发生错误时即关闭该连接
    virtual wStatus Handlemsg(char *buf[], uint32_t len) {
        return mStatus = wStatus::IOError("wTask::Handlemsg, socket will be closed", "method should be inherit");
    }
    
    // 异步发送：将待发送客户端消息写入buf，等待TaskSend发送
    // -1 ：消息长度不合法
    // -2 ：发送缓冲剩余空间不足，请稍后重试
    // 0 : 发送成功
    wStatus Send2Buf(const char buf[], size_t len);

    // 同步发送确切长度消息
    wStatus SyncSend(const char buf[], size_t len, ssize_t *size);
    
    // 同步接受确切长度消息(需保证此sock未加入epoll中，防止出现竞争！！)
    // 调用者确保vCmd有足够长的空间接受自此同步消息
    wStatus SyncRecv(char buf[], size_t len, size_t *size, uint32_t timeout /*s*/);

    size_t WritableLen() {
        return mSendLen;
    }

    wSocket *Socket() {
        return mSocket;
    }
   
    wStatus HeartbeatSend();

    void HeartbeatReset() {
        return mHeartbeat = 0;
    }

    bool HeartbeatOuttimes() {
        return mHeartbeat > kHeartbeat;
    }
   
    /*
    bool IsRunning() {
        return mStatus == TASK_RUNNING;
    }
    */
   
    //TASK_STATUS &Status() { return mStatus;}
protected:
    int8_t mState;
    wStatus mStatus;
    wSocket	*mSocket;
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
};

}	// namespace hnet

#endif