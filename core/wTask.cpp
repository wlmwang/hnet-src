
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wTask.h"
#include "wCommand.h"
#include "wMisc.h"
#include "wSocket.h"

namespace hnet {

wTask::wTask(wSocket* socket, int32_t type) : mSocket(socket), mType(type), mHeartbeat(0), mRecvLen(0), mSendLen(0),
mRecvRead(mRecvBuff), mRecvWrite(mRecvBuff), mSendRead(mSendBuff), mSendWrite(mSendBuff) { }

wTask::~wTask() {
    SAFE_DELETE(mSocket);
}

wStatus wTask::HeartbeatSend() {
    mHeartbeat++;
    struct wCommand cmd;
    return SyncSend((char *)&cmd, sizeof(cmd));
}

wStatus wTask::TaskRecv(ssize_t *size) {
    char *buffend = mRecvBuff + kPackageSize;

    // 写指针移动到结尾，回退到开始
    if (mRecvRead != mRecvBuff && mRecvWrite == buffend) {
        mRecvWrite = mRecvBuff;
    }

    // len >= 0 正向剩余
    // len < 0 分段，中间剩余
    ssize_t len =  static_cast<ssize_t>(mRecvWrite-mRecvRead);
    size_t leftlen = len>=0? static_cast<size_t>(buffend - mRecvWrite): static_cast<size_t>(-len);

    mStatus = mSocket->RecvBytes(mRecvWrite, leftlen, size);
    if (!mStatus.Ok()) {
        return mStatus;
    }
    mRecvLen += *size;
    mRecvWrite += *size;
   
    LOG_DEBUG(kErrLogKey, "[system] recv buf(%d-%d) rw(%d-%d) len(%d) leftlen(%d) movelen(%d) recvLen(%d) *size(%d)",
        mRecvBuff, buffend, mRecvRead, mRecvWrite, len, leftlen, mRecvRead-mRecvBuff, mRecvLen, *size);
    
    // 消息解析    
    auto handleFunc = [this] (char* buf, uint32_t len, char* nextbuf) {
        this->mStatus = Handlemsg(buf, len);
        this->mRecvRead = nextbuf;
        this->mRecvLen -= len + sizeof(int);
    };

    while (mRecvLen > 0) {
        if (mRecvRead == buffend && mRecvWrite != mRecvBuff) {
            mRecvRead = mRecvBuff;
        }
        len =  static_cast<ssize_t>(mRecvWrite-mRecvRead);
        
        uint32_t reallen = 0;
        if (len > 0) {
            // 消息字符在正向缓冲中
            // 
            // 消息长度
            reallen = coding::DecodeFixed32(mRecvRead);
            if (reallen < kMinPackageSize || reallen > kMaxPackageSize) {
                mStatus = wStatus::IOError("wTask::TaskRecv, message length error", "out range");
                break;
            } else if (reallen > static_cast<uint32_t>(len - sizeof(int))) {
                // recv a part of message: real len = %d, now len = %d
                mStatus = wStatus::Nothing();
                break;
            }

            handleFunc(mRecvRead + sizeof(int), reallen, mRecvRead + sizeof(int) + len);
            if (!mStatus.Ok()) {
                break;
            }
        } else {
            leftlen = buffend - mRecvRead;
            // 消息字符分段存储
            // 
            // 消息长度
            if (leftlen >= sizeof(int)) {
                reallen = coding::DecodeFixed32(mRecvRead);
            } else {
                // 长度分段
                [=, &reallen]() {
                    for (size_t pos = 0; pos < leftlen; pos++) {
                        reallen |= (static_cast<uint32_t>(static_cast<unsigned char>(mRecvRead[pos])) << (8 * pos));
                    }
                    for (size_t pos = 0; pos < sizeof(int) - leftlen; pos++) {
                        reallen |= (static_cast<uint32_t>(static_cast<unsigned char>(mRecvBuff[pos])) << (8 * pos));
                    }
                } ();
            }

            if (reallen < kMinPackageSize || reallen > kMaxPackageSize) {
                mStatus = wStatus::IOError("wTask::TaskRecv, message length error", "out range");
                break;
            } else if (reallen > (int)(kPackageSize + len - sizeof(int))) {
                // recv a part of message: real len = %d, now len = %d
                mStatus = wStatus::Nothing();
                break;
            }
            
            if (reallen <= static_cast<uint32_t>(leftlen - sizeof(int))) {
                // 正向剩余
                handleFunc(mRecvRead + sizeof(int), reallen, mRecvRead + sizeof(int) + len);
                if (!mStatus.Ok()) {
                    break;
                }
            } else {
                memcpy(mTempBuff, mRecvRead, leftlen);
                memcpy(mTempBuff + leftlen, mRecvBuff, reallen - leftlen + sizeof(int));
                handleFunc(mTempBuff + sizeof(int), reallen, mRecvBuff + (reallen - leftlen + sizeof(int)));
                if (!mStatus.Ok()) {
                    break;
                }
            }
        }
    }
    
    return mStatus;
}

wStatus wTask::TaskSend(ssize_t *size) {
    char *buffend = mSendBuff + kPackageSize;

    while (true) {
        ssize_t len = mSendWrite - mSendRead;
        if (mSendLen == 0) {
            mStatus = wStatus::Nothing();
            break;
        } else if (len > 0) {
            mStatus = mSocket->SendBytes(mSendRead, mSendLen, size); //len == mSendLen
            if (!mStatus.Ok()) {
                break;
            }
            mSendLen -= *size;
            mSendRead += *size;
        } else if (len <= 0) {
            ssize_t leftlen = buffend - mSendRead;
            memcpy(mTmpBuff, mSendRead, leftlen);
            memcpy(mTmpBuff + leftlen, mSendBuff, mSendLen - leftlen);
            mStatus = mSocket->SendBytes(mTmpBuff, mSendLen, size);
            if (!mStatus.Ok()) {
                break;
            }
            mSendLen -= *size;
            mSendRead = mSendBuff + mSendLen - leftlen;
        }
    }
    return mStatus;
}

wStatus wTask::Send2Buf(const char buf[], size_t len) {
    //判断消息长度
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::IOError("wTask::Send2Buf, message length error", "out range");
    } else if (len > (int)(kPackageSize - mSendLen - sizeof(int))) {
        return mStatus = wStatus::IOError("wTask::Send2Buf, message length error", "left buf not enough");
    }

    char *buffend = mSendBuff + kPackageSize;
    if (mSendRead != mSendBuff && mSendWrite == buffend) {
        // 回退可写指针
        mSendWrite = mSendBuff;
    }
    
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    if ((writelen >= 0 && leftlen >= len + sizeof(int)) || (writelen < 0 && -writelen >= len + sizeof(int))) {
        memcpy(mSendWrite, &len, sizeof(int));
        memcpy(mSendWrite + sizeof(int), buf, len);
        mSendWrite += len + sizeof(int);
    } else if (writelen >= 0 && leftlen < (len + sizeof(int))) {
        memcpy(mTmpBuff, &len, sizeof(int));
        memcpy(mTmpBuff + sizeof(int), buf, len);
        memcpy(mSendWrite, mTmpBuff, leftlen);
        memcpy(mSendBuff, mTmpBuff + leftlen, len + sizeof(int) - leftlen);
        mSendWrite = mSendBuff + len + sizeof(int) - leftlen;
    }
    mSendLen += len + sizeof(int);
    return mStatus = wStatus::Nothing();
}

wStatus wTask::SyncSend(const char buf[], size_t len, ssize_t *size) {
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::IOError("wTask::SyncSend, message length error", "out range");
    }
    
    memcpy(mTmpBuff, &len, sizeof(int));
    memcpy(mTmpBuff + sizeof(int), buf, len);
    return mStatus = mSocket->SendBytes(mTmpBuff, len + sizeof(int), size);
}

wStatus wTask::SyncRecv(char buf[], size_t len, size_t *size, uint32_t timeout) {
    uint64_t sleepusc = 100;
    uint64_t trycnt = timeout*1000000 / sleepusc;
    
    struct wCommand* cmd = NULL;
    size_t cmdlen = sizeof(int) + sizeof(struct wCommand);
    
    size_t recvlen = 0;
    while (true) {
        mStatus = mSocket->RecvBytes(mTmpBuff + recvlen, len + sizeof(int) - recvlen, size);
        if (!mStatus.Ok()) {
            return mStatus;
        }
        recvlen += *size;
        
        if (recvlen < cmdlen) {
            if (trycnt-- < 0) {
                break;
            }
            usleep(sleepusc);
            continue;
        }

        // 忽略心跳包干扰
        cmd = reinterpret_cast<struct wCommand*>(mTmpBuff + sizeof(int));
        if (cmd != NULL && cmd->GetCmd() == CMD_NULL && cmd->GetPara() == PARA_NULL) {
            memmove(mTmpBuff, mTmpBuff + cmdlen, recvlen -= cmdlen);
            continue;
        }

        if ((recvlen < len + sizeof(int)) && (trycnt-- > 0)) {
            usleep(sleepusc);
            continue;
        } else if (trycnt < 0) {
            mStatus = wStatus::IOError("wTask::SyncRecv, recv message error", "timeout");
            break;
        } else {
            mStatus = wStatus::Nothing();
            break;
        }
    }
    
    uint32_t msglen = coding::DecodeFixed32(mTmpBuff);
    if (msglen < kMinPackageSize || msglen > kMaxPackageSize) {
        return mStatus = wStatus::IOError("wTask::SyncRecv, message length error", "out range");
    } else if (msglen > static_cast<uint32_t>(recvlen - sizeof(int)))	{
        return mStatus = wStatus::IOError("wTask::SyncRecv, message length error", "sync recv a part of message");
    } else if (msglen != len) {
        return mStatus = wStatus::IOError("wTask::SyncRecv, message length error", "sync recv error buffer len");
    }

    *size = recvlen;
    memcpy(buf, mTmpBuff + sizeof(int), recvlen);
    return mStatus = wStatus::Nothing();
}

wStatus wTask::Handlemsg(char* buf[], uint32_t len) {
    struct wCommand *cmd = reinterpret_cast<struct wCommand*>(buf);
    
    if (cmd->GetId() == CmdId(CMD_NULL, PARA_NULL)) {
    	mHeartbeat = 0;
    	mStatus = wStatus::Nothing();
    } else {
    	auto pf = mDispatch.GetFuncT(Name(), cmd->GetId());
    	if (pf != NULL) {
    	    if (pf->mFunc(buf, len) == 0) {
                mStatus = wStatus::Nothing();
            } else {
                //mStatus = wStatus::IOError("wTask::Handlemsg, result error", "not return 0");
            }
    	} else {
    	    mStatus = wStatus::IOError("wTask::Handlemsg, invalid msg", "no method find");
    	}
    }
    return mStatus;
}

inline const char* wTask::Name() const {
    return "wTask";
}

/*
wStatus wTask::Login() {
    ssize_t size;
    struct LoginReqToken_t loginReq;
    memcpy(loginReq.mToken, kToken, 4);
    // 登录请求
    if (SyncSend(reinterpret_cast<const char*>(&loginReq), sizeof(struct LoginReqToken_t), &size).Ok()) {
        char buf[sizeof(struct LoginReqToken_t)];
        // 登录响应
        if (SyncRecv(buf, sizeof(struct LoginReqToken_t), &size).Ok()) {
            struct LoginReqToken_t *loginRes = reinterpret_cast<struct LoginReqToken_t*>(buf);
            if (strcmp(loginRes->mToken, kToken) == 0) {
                return mStatus = wStatus::Nothing();
            }
        }
    }
    return mStatus = wStatus::IOError("wTask::Login, login failed", "token error");
}
*/

}   // namespace hnet
