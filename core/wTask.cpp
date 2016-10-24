
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wTask.h"
#include "wCommand.h"
#include "wMisc.h"
#include "wSocket.h"
#include "wServer.h"
#include "wMultiClient.h"

namespace hnet {

wTask::wTask(wSocket* socket, int32_t type) : mType(type), mSocket(socket), mHeartbeat(0), 
mRecvRead(mRecvBuff), mRecvWrite(mRecvBuff), mRecvLen(0), mSendRead(mSendBuff), mSendWrite(mSendBuff),
mSendLen(0), mServer(NULL), mClient(NULL), mSCType(0) { }

wTask::~wTask() {
    SAFE_DELETE(mSocket);
}

wStatus wTask::HeartbeatSend() {
    mHeartbeat++;
    struct wCommand cmd;
    return AsyncSend(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

wStatus wTask::TaskRecv(ssize_t *size) {
	const char *buffend = mRecvBuff + kPackageSize;
	// 循环队列
	if (mRecvLen < kPackageSize && mRecvLen == 0) {
		mRecvRead = mRecvWrite = mRecvBuff;
	} else if (mRecvLen < kPackageSize && mRecvRead != mRecvBuff && mRecvWrite == buffend) {
		mRecvWrite = mRecvBuff;
	}

    // len >= 0 正向剩余
    // len < 0 分段，中间剩余
    ssize_t len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
    size_t leftlen = len>=0? static_cast<size_t>(buffend - mRecvWrite): static_cast<size_t>(abs(len));

    // socket接受数据
    *size = 0;
    if (leftlen != 0 && mRecvLen < kPackageSize) {
        mStatus = mSocket->RecvBytes(mRecvWrite, leftlen, size);
        if (!mStatus.Ok()) {
            return mStatus;
        }
        mRecvLen += (*size);
        mRecvWrite += (*size);
    }

    // 消息解析
    auto handleFunc = [this] (char* buf, uint32_t len, char* nextbuf) {
        this->mStatus = this->Handlemsg(buf, len);
        this->mRecvRead = nextbuf;
        this->mRecvLen -= (len + sizeof(uint32_t));
    };

    while (mRecvLen > sizeof(uint32_t)) {
    	// 循环队列
    	if (mRecvRead == buffend && mRecvWrite != mRecvBuff) {
            mRecvRead = mRecvBuff;
        }
    	ssize_t len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
        
        uint32_t reallen = 0;
        if (len > 0) {
            // 消息字符在正向缓冲中
            reallen = coding::DecodeFixed32(mRecvRead);
            if (reallen < kMinPackageSize || reallen > kMaxPackageSize) {
                mStatus = wStatus::IOError("wTask::TaskRecv, message length error", "out range");
                break;
            } else if (reallen > static_cast<uint32_t>(len - sizeof(uint32_t))) {
                // recv a part of message: real len = %d, now len = %d
                mStatus = wStatus::Nothing();
                break;
            }

            handleFunc(mRecvRead + sizeof(uint32_t), reallen, mRecvRead + sizeof(uint32_t) + reallen);
            if (!mStatus.Ok()) {
                break;
            }
        } else {
            leftlen = buffend - mRecvRead;
            // 消息字符分段存储
            if (leftlen >= sizeof(uint32_t)) {
                reallen = coding::DecodeFixed32(mRecvRead);
            } else {
                // 长度分段
                [=, &reallen]() {
                    for (size_t pos = 0; pos < leftlen; pos++) {
                        reallen |= (static_cast<uint32_t>(static_cast<unsigned char>(mRecvRead[pos])) << (8 * pos));
                    }
                    for (size_t pos = 0; pos < sizeof(uint32_t) - leftlen; pos++) {
                        reallen |= (static_cast<uint32_t>(static_cast<unsigned char>(mRecvBuff[pos])) << (8 * pos));
                    }
                } ();
            }

            if (reallen < kMinPackageSize || reallen > kMaxPackageSize) {
                mStatus = wStatus::IOError("wTask::TaskRecv, message length error", "message too large");
                break;
            } else if (reallen > static_cast<uint32_t>(kPackageSize - abs(len) - sizeof(uint32_t))) {
                // recv a part of message: real len = %d, now len = %d
                mStatus = wStatus::Nothing();
                break;
            }
            
            if (reallen <= static_cast<uint32_t>(leftlen - sizeof(uint32_t))) {
                // 正向剩余
                handleFunc(mRecvRead + sizeof(uint32_t), reallen, mRecvRead + sizeof(uint32_t) + reallen);
                if (!mStatus.Ok()) {
                    break;
                }
            } else {
                memcpy(mTempBuff, mRecvRead, leftlen);
                memcpy(mTempBuff + leftlen, mRecvBuff, reallen - leftlen + sizeof(uint32_t));
                handleFunc(mTempBuff + sizeof(uint32_t), reallen, mRecvBuff + (reallen - leftlen + sizeof(uint32_t)));
                if (!mStatus.Ok()) {
                    break;
                }
            }
        }
    }
    
    return mStatus;
}

wStatus wTask::TaskSend(ssize_t *size) {
    ssize_t len;
    const char *buffend = mSendBuff + kPackageSize;
    while (true) {
        len = mSendWrite - mSendRead;
        if (mSendLen == 0) {
            mStatus = wStatus::Nothing();
            break;
        } else if (len > 0) {
            mStatus = mSocket->SendBytes(mSendRead, mSendLen, size); //len == mSendLen
            if (!mStatus.Ok()) {
                break;
            }
            mSendLen -= (*size);
            mSendRead += (*size);
        } else if (len <= 0) {
            ssize_t leftlen = buffend - mSendRead;
            memcpy(mTempBuff, mSendRead, leftlen);
            memcpy(mTempBuff + leftlen, mSendBuff, mSendLen - leftlen);
            mStatus = mSocket->SendBytes(mTempBuff, mSendLen, size);
            if (!mStatus.Ok()) {
                break;
            }
            mSendLen -= (*size);
            if (*size <= leftlen) {
            	mSendRead += (*size);
            } else {
            	mSendRead = mSendBuff + mSendLen - leftlen;
            }
        }
    }
    return mStatus;
}

wStatus wTask::Send2Buf(char cmd[], size_t len) {
    // 判断消息长度
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::IOError("wTask::Send2Buf, message length error", "message too large");
    } else if (len > static_cast<size_t>(kPackageSize - mSendLen - sizeof(uint32_t))) {
        return mStatus = wStatus::IOError("wTask::Send2Buf, message length error", "left buffer not enough");
    }

    const char *buffend = mSendBuff + kPackageSize;
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    if ((writelen >= 0 && leftlen >= static_cast<ssize_t>(len + sizeof(uint32_t))) || (writelen < 0 && abs(writelen) >= static_cast<ssize_t>(len + sizeof(int)))) {
    	coding::EncodeFixed32(mSendWrite, static_cast<uint32_t>(len));
    	memcpy(mSendWrite += sizeof(uint32_t), cmd, len);
    	mSendWrite += len;
    } else if (writelen >= 0 && leftlen < static_cast<ssize_t>(len + sizeof(uint32_t))) {
    	coding::EncodeFixed32(mTempBuff, static_cast<uint32_t>(len));
    	memcpy(mTempBuff + sizeof(uint32_t), cmd, len);

    	memcpy(mSendWrite, mTempBuff, leftlen);
    	mSendWrite = mSendBuff;
    	memcpy(mSendWrite, mTempBuff + leftlen, sizeof(uint32_t) + len - leftlen);
    	mSendWrite += sizeof(uint32_t)+ len - leftlen;
    } else {
    	return mStatus = wStatus::IOError("wTask::Send2Buf, message length error", "left buffer not enough");
    }

    mSendLen += len + sizeof(uint32_t);
    return mStatus = wStatus::Nothing();
}

wStatus wTask::SyncSend(char cmd[], size_t len, ssize_t *size) {
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::IOError("wTask::SyncSend, message length error", "out range");
    }
    coding::EncodeFixed32(mTempBuff, static_cast<uint32_t>(len));
    memcpy(mTempBuff + sizeof(uint32_t), cmd, len);
    return mStatus = mSocket->SendBytes(mTempBuff, len + sizeof(uint32_t), size);
}

wStatus wTask::AsyncSend(char cmd[], size_t len) {
	if (mSCType == 0 && mServer != NULL) {
		mStatus = mServer->Send(this, cmd, len);
	} else if (mSCType == 1 && mClient != NULL) {
		mStatus = mClient->Send(this, cmd, len);
	} else {
		mStatus = wStatus::IOError("wTask::AsyncSend, failed", "mServer or mClient is null");
	}
	return mStatus;
}

wStatus wTask::SyncRecv(char cmd[], size_t len, ssize_t *size, uint32_t timeout) {
    uint64_t sleepusc = 100;
    uint64_t trycnt = timeout*1000000 / sleepusc;
    
    struct wCommand* nullcmd = NULL;
    size_t cmdlen = sizeof(uint32_t) + sizeof(struct wCommand);
    size_t recvlen = 0;
    while (true) {
        mStatus = mSocket->RecvBytes(mTempBuff + recvlen, len + sizeof(uint32_t) - recvlen, size);
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
        nullcmd = reinterpret_cast<struct wCommand*>(mTempBuff + sizeof(uint32_t));
        if (nullcmd != NULL && nullcmd->GetCmd() == kCmdNull && nullcmd->GetPara() == kParaNull) {
            memmove(mTempBuff, mTempBuff + cmdlen, recvlen -= cmdlen);
            continue;
        }

        if ((recvlen < len + sizeof(uint32_t)) && (trycnt-- > 0)) {
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
    
    uint32_t msglen = coding::DecodeFixed32(mTempBuff);
    if (msglen < kMinPackageSize || msglen > kMaxPackageSize) {
        return mStatus = wStatus::IOError("wTask::SyncRecv, message length error", "out range");
    } else if (msglen > static_cast<uint32_t>(recvlen - sizeof(uint32_t)))	{
        return mStatus = wStatus::IOError("wTask::SyncRecv, message length error", "sync recv a part of message");
    } else if (msglen != len) {
        return mStatus = wStatus::IOError("wTask::SyncRecv, message length error", "sync recv error buffer len");
    }

    *size = recvlen;
    memcpy(cmd, mTempBuff + sizeof(uint32_t), recvlen);
    return mStatus = wStatus::Nothing();
}

wStatus wTask::Handlemsg(char cmd[], uint32_t len) {
	struct wCommand *basecmd = reinterpret_cast<struct wCommand*>(cmd);
	if (basecmd->GetId() == CmdId(kCmdNull, kParaNull)) {
		mHeartbeat = 0;
		mStatus = wStatus::Nothing();
	} else {
		struct Request_t request(cmd, len);
		if (mEvent(basecmd->GetId(), &request) == false) {
			mStatus = wStatus::IOError("wTask::Handlemsg, invalid request", "no method find");
		}
	}
	return mStatus;
}

}   // namespace hnet
