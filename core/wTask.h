
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

    wSocket *Socket() { return mSocket;}
    //TASK_STATUS &Status() { return mStatus;}
    bool IsRunning() { return mStatus == TASK_RUNNING;}

    virtual int VerifyConn() { return 0;}	//验证接收到连接
    virtual int Verify() {return 0;}		//发送连接验证请求

    // iReason关闭原因
    virtual void CloseTask(int iReason = 0) { }

    virtual int Heartbeat();
    
    virtual int HeartbeatOutTimes() {
        return mHeartbeat > kKeepAliveCnt;
    }

    virtual int ClearbeatOutTimes() {
        return mHeartbeat = 0;
    }

    /**
     *  处理接受到数据
     *  每条消息大小[1b,512k]
     *  核心逻辑：接受整条消息，然后进入用户定义的业务函数HandleRecvMessage
     *  return ：<0 对端发生错误|消息超长|对端关闭(FIN_WAIT) =0 稍后重试 >0 接受字符
     */
    virtual int TaskRecv();
    virtual int TaskSend();
    /**
     * 业务逻辑入口函数
     */
    virtual int HandleRecvMessage(char *pBuffer, int nLen) { return -1;}
    /**
     * 发送缓冲区是否有数据
     */
    int WritableLen() { return mSendLen;}
    /**
     *  将待发送客户端消息写入buf，等待TaskSend发送
     *  return 
     *  -1 ：消息长度不合法
     *  -2 ：发送缓冲剩余空间不足，请稍后重试
     *   0 : 发送成功
     */
    int Send2Buf(const char *pCmd, int iLen);
    /**
     *  同步发送确切长度消息
     */
    int SyncSend(const char *pCmd, int iLen);
    /**
     *  同步接受确切长度消息(需保证此sock未加入epoll中，防止出现竞争！！)
     *  确保pCmd有足够长的空间接受自此同步消息
     */
    int SyncRecv(char vCmd[], int iLen, int iTimeout = 10/*s*/);

protected:
    wStatus mStatus;
    wSocket	*mSocket;
    // TASK_STATUS mStatus {TASK_INIT};
    uint8_t mHeartbeat;

    char mSyncBuff[kPackageSize];    // 同步发送、接受消息缓冲
    char mRecvBuff[kPackageSize];    // 异步接受消息缓冲
    char mSendBuff[kPackageSize];    // 异步发送消息缓冲
    
    char *mRecvWrite;
    char *mRecvRead;
    int  mRecvLen;

    char *mSendWrite;
    char *mSendRead;
    int  mSendLen;  //可发送数据长度
};

}	// namespace hnet

#endif