
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wTask.h"
#include "wCommand.h"
#include "wLog.h"
#include "wMisc.h"
#include "wSocket.h"

namespace hnet {

wTask::wTask() : mSocket(NULL), mHeartbeat(0), mRecvLen(0), mSendLen(0),
mRecvRead(mRecvBuff), mRecvWrite(mRecvBuff), mSendRead(mSendBuff), mSendWrite(mSendBuff) { }

wTask::wTask(wSocket* pSocket) : mSocket(pSocket), mHeartbeat(0), mRecvLen(0), mSendLen(0),
mRecvRead(mRecvBuff), mRecvWrite(mRecvBuff), mSendRead(mSendBuff), mSendWrite(mSendBuff) { }

wTask::~wTask() {
    misc::SafeDelete<wSocket>(mSocket);
}

int wTask::Heartbeat() {
    mHeartbeat++;
    struct wCommand cmd;
    return SyncSend((char *)&cmd, sizeof(cmd));
}

int wTask::TaskRecv() {
    // 超尾指针
    char *pBuffEnd = mRecvBuff + kPackageSize;
    
    if (mRecvRead != mRecvBuff && mRecvWrite == pBuffEnd) {
        // 回退可写指针
        mRecvWrite = mRecvBuff;
    }

    int iLeftLen;
    int iLen =  mRecvWrite - mRecvRead;
    if (iLen >= 0) {
        // 正向剩余
        iLeftLen = pBuffEnd - mRecvWrite;
    } else {
        // 分段，中间剩余
        iLeftLen = -iLen;
    }

    int iRecvLen = 0;
    if (mRecvLen < kPackageSize) {
        // 接受socket数据
        iRecvLen = mSocket->RecvBytes(mRecvWrite, iLeftLen);
        if (iRecvLen <= 0) {
            return iRecvLen;
        }

        mRecvLen += iRecvLen;
        mRecvWrite += iRecvLen;
    }

    //LOG_DEBUG(ELOG_KEY, "[system] recv buf(%d-%d) rw(%d-%d) iLen(%d) leftlen(%d) movelen(%d) recvLen(%d) iRecvLen(%d)",
    //    mRecvBuff, pBuffEnd, mRecvRead, mRecvWrite, iLen, iLeftLen, mRecvRead-mRecvBuff, mRecvLen, iRecvLen);

    for (int iRealLen = 0; mRecvLen > 0;) {
        if (mRecvRead == pBuffEnd && mRecvWrite != mRecvBuff) {
            mRecvRead = mRecvBuff;
        }
        iLen =  mRecvWrite - mRecvRead;
        
        if (iLen > 0) {
            // 正向解析
            memcpy(&iRealLen, mRecvRead, sizeof(iRealLen));
            if (iRealLen < kMinPackageSize || iRealLen > kMaxPackageSize) {
                //LOG_ERROR(ELOG_KEY, "[system] recv message invalid len: %d , fd(%d)", iRealLen, mSocket->FD());
                return kSeMsgLen;
            } else if (iRealLen > (int) (iLen - sizeof(int))) {
                //LOG_DEBUG(ELOG_KEY, "[system] recv a part of message: real len = %d, now len = %d", iRealLen, iLen - sizeof(int));
                return 0;
            }

            // 消息解析，去除4字节消息长度标识位
            HandleRecvMessage(mRecvRead + sizeof(int), iRealLen);
            mRecvLen -= iRealLen + sizeof(int);
            mRecvRead += iRealLen + sizeof(int);
        } else {
            iLeftLen = pBuffEnd - mRecvRead;
            if (iLeftLen >= (int)sizeof(int)) {
                memcpy(&iRealLen, mRecvRead, sizeof(iRealLen));
            } else {
                int tmp;
                iRealLen = 0;
                for (int i = iLeftLen; i > 0; i--) {
                    tmp = *(mRecvRead + (sizeof(int) - i -1));
                    iRealLen |= tmp << (8 * (i + 1));
                }
                for (int i = sizeof(int) - iLeftLen; i > 0; i--) {
                    tmp = *(mRecvRead + (sizeof(int) - i -1));
                    iRealLen |= tmp << (8 * (i + 1));
                }
            }

            if (iRealLen < kMinPackageSize || iRealLen > kMaxPackageSize) {
                //LOG_ERROR(ELOG_KEY, "[system] recv message invalid len: %d , fd(%d)", iRealLen, mSocket->FD());
                return kSeMsgLen;
            } else if (iRealLen > (int)(kPackageSize + iLen - sizeof(int))) {
                //LOG_DEBUG(ELOG_KEY, "[system] recv a part of message: real len = %d, now len = %d", iRealLen, kPackageSize + iLen - sizeof(int));
                return 0;
            }
            
            if (iRealLen <= (int)(iLeftLen - sizeof(int))) {
                // 正向剩余
                // 消息解析，去除4字节消息长度标识位
                HandleRecvMessage(mRecvRead + sizeof(int), iRealLen);
                mRecvLen -= iRealLen + sizeof(int);
                mRecvRead += iRealLen + sizeof(int);
            } else {
                memcpy(mTmpBuff, mRecvRead, iLeftLen);
                memcpy(mTmpBuff + iLeftLen, mRecvBuff, iRealLen - iLeftLen + sizeof(int));
                
                // 消息解析，去除4字节消息长度标识位
                HandleRecvMessage(mTmpBuff + sizeof(int), iRealLen);
                mRecvLen -= iRealLen + sizeof(int);
                mRecvRead = mRecvBuff + (iRealLen - iLeftLen + sizeof(int));
            }
        }
    }
    
    return iRecvLen;
}

int wTask::TaskSend() {
    char *pBuffEnd = mSendBuff + kPackageSize;
    int iLen = 0, iSendLen = 0, iLeftLen = 0;
    while (true) {
        iLen = mSendWrite - mSendRead;
        if (mSendLen == 0) {
            return 0;
        } else if (iLen > 0) {
            iSendLen = mSocket->SendBytes(mSendRead, mSendLen); //iLen == mSendLen
            
            if (iSendLen < 0) return iSendLen;
            mSendLen -= iSendLen;
            mSendRead += iSendLen;
        } else if (iLen <= 0) {
            iLeftLen = pBuffEnd - mSendRead;
            memcpy(mTmpBuff, mSendRead, iLeftLen);
            memcpy(mTmpBuff + iLeftLen, mSendBuff, mSendLen - iLeftLen);
            iSendLen = mSocket->SendBytes(mTmpBuff, mSendLen);
            
            if (iSendLen < 0) return iSendLen;
            mSendLen -= iSendLen;
            mSendRead = mSendBuff + mSendLen - iLeftLen;
        }
        
        //LOG_DEBUG(ELOG_KEY, "[system] send message len: %d, fd(%d)", iSendLen, mSocket->FD());
    }
    
    return iSendLen;
}

int wTask::Send2Buf(const char *pCmd, int iLen) {
    //判断消息长度
    if (iLen < kMinPackageSize || iLen > kMaxPackageSize) {
        //LOG_ERROR(ELOG_KEY, "[system] write message invalid len: %d , fd(%d)", iLen, mSocket->FD());
        return kSeMsgLen;
    } else if (iLen > (int)(kPackageSize - mSendLen - sizeof(int))) {
        //LOG_ERROR(ELOG_KEY, "[system] send buf not enough.left buf(%d) need(%d)", kPackageSize - mSendLen, iLen + sizeof(int));
        return kSeNobuff;
    }

    char *pBuffEnd = mSendBuff + kPackageSize;
    if (mSendRead != mSendBuff && mSendWrite == pBuffEnd) {
        // 回退可写指针
        mSendWrite = mSendBuff;
    }
    
    int iWrLen =  mSendWrite - mSendRead;
    int iLeftLen = pBuffEnd - mSendWrite;
    if ((iWrLen >= 0 && iLeftLen >= (int)(iLen + sizeof(int))) || (iWrLen < 0 && -iWrLen >= (int)(iLen + sizeof(int)))) {
        memcpy(mSendWrite, &iLen, sizeof(int));
        memcpy(mSendWrite + sizeof(int), pCmd, iLen);
        mSendWrite += iLen + sizeof(int);
    } else if (iWrLen >= 0 && iLeftLen < (int)(iLen + sizeof(int))) {
        memcpy(mTmpBuff, &iLen, sizeof(int));
        memcpy(mTmpBuff + sizeof(int), pCmd, iLen);
        memcpy(mSendWrite, mTmpBuff, iLeftLen);
        memcpy(mSendBuff, mTmpBuff + iLeftLen, iLen + sizeof(int) - iLeftLen);
        mSendWrite = mSendBuff + iLen + sizeof(int) - iLeftLen;
    }
    mSendLen += iLen + sizeof(int);
    
    //LOG_DEBUG(ELOG_KEY, "[system] write message to left buf(%d), message len(%d)", kPackageSize - mSendLen, iLen);
    return iLen;
}

int wTask::SyncSend(const char *pCmd, int iLen) {
    //判断消息长度
    if ((iLen < kMinPackageSize) || (iLen > kMaxPackageSize)) {
        //LOG_ERROR(ELOG_KEY, "[system] write message invalid len: %d , fd(%d)", iLen, mSocket->FD());
        return kSeMsgLen;
    }
    
    memcpy(mTmpBuff, &iLen, sizeof(int));
    memcpy(mTmpBuff + sizeof(int), pCmd, iLen);
    return mSocket->SendBytes(mTmpBuff, iLen + sizeof(int));
}

int wTask::SyncRecv(char vCmd[], int iLen, int iTimeout) {
    long long iSleep = 100;
    int iTryCount = iTimeout*1000000 / iSleep;
    int iCmdMsgLen = sizeof(int) + sizeof(struct wCommand);
    struct wCommand* pTmpCmd = 0;
    
    int iSize = 0, iRecvLen = 0;
    while (true) {
        iSize = mSocket->RecvBytes(mTmpBuff + iRecvLen, iLen + sizeof(int) - iRecvLen);
        if (iSize < 0) break;
        iRecvLen += iSize;
        
        if ((iSize == 0) || (iRecvLen < iCmdMsgLen)) {
            if (iTryCount-- < 0) break;
            usleep(iSleep);
            continue;
        }

        pTmpCmd = (struct wCommand*) (mTmpBuff + sizeof(int));	//心跳包干扰
        if (pTmpCmd != NULL && pTmpCmd->GetCmd() == CMD_NULL && pTmpCmd->GetPara() == PARA_NULL) {
            memmove(mTmpBuff, mTmpBuff + iCmdMsgLen, iRecvLen -= iCmdMsgLen);
            continue;
        }

        if (((size_t)iRecvLen < iLen + sizeof(int)) && (iTryCount-- > 0)) {
            usleep(iSleep);
            continue;
        }
        break;
    }
    
    int iMsgLen;
    memcpy(&iMsgLen, mTmpBuff, sizeof(iMsgLen));
    if ((iRecvLen <= 0) || (iMsgLen < kMinPackageSize) || (iMsgLen > kMaxPackageSize)) {
        //LOG_ERROR(ELOG_KEY, "[system] sync recv message invalid len: %d, fd(%d)", iMsgLen, mSocket->FD());
        return kSeMsgLen;
    } else if (iMsgLen > (int)(iRecvLen - sizeof(int)))	{   //消息不完整 
        //LOG_DEBUG(ELOG_KEY, "[system] sync recv a part of message: real len = %d, now len = %d, call len = %d", iMsgLen, iRecvLen, iLen);
        return kSeMsgLen;
    } else if (iMsgLen != iLen) {
        //LOG_DEBUG(ELOG_KEY, "[system] sync recv error buffer len");
        return kSeMsgLen;
    }
    
    memcpy(vCmd, mTmpBuff + sizeof(int), iLen);
    return iRecvLen - sizeof(int);
}

}   // namespace hnet
