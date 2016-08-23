
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TASK_H_
#define _W_TASK_H_

#include "wCore.h"
#include "wCommand.h"
#include "wLog.h"
#include "wMisc.h"
#include "wNoncopyable.h"
#include "wSocket.h"

class wTask : private wNoncopyable
{
	public:
		wTask();
		wTask(wSocket *pSocket);
		virtual ~wTask();
                void Initialize();
                
		wSocket *Socket() { return mSocket;}
		TASK_STATUS &Status() { return mStatus;}
		bool IsRunning() { return mStatus == TASK_RUNNING;}
		
		virtual int VerifyConn() { return 0;}	//验证接收到连接
		virtual int Verify() {return 0;}		//发送连接验证请求

		virtual void CloseTask(int iReason = 0) {}	//iReason关闭原因
		
		virtual int Heartbeat();
		virtual int HeartbeatOutTimes() { return mHeartbeatTimes > KEEPALIVE_CNT; }
		virtual int ClearbeatOutTimes() { return mHeartbeatTimes = 0; }
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
		wSocket	*mSocket {NULL};
		TASK_STATUS mStatus {TASK_INIT};
		int mHeartbeatTimes {0};
		
		//临时缓冲区
        char mTmpBuff[MSG_BUFF_LEN] {'\0'};
        
        //接收消息的缓冲区 512K
        char mRecvBuff[MSG_BUFF_LEN] {'\0'};
        char *mRecvWrite {NULL};
        char *mRecvRead {NULL};
        int  mRecvLen {0}; //已接收数据长度
        
        //接收消息的缓冲区 512K
        char mSendBuff[MSG_BUFF_LEN] {'\0'};
        char *mSendWrite {NULL};
        char *mSendRead {NULL};
        int  mSendLen {0};  //可发送数据长度
};

#endif

