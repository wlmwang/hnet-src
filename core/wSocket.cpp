
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSocket.h"
#include "wMisc.h"

namespace hnet {

wSocket::wSocket(SockType type, SockProto proto, SockFlag flag) : mFD(kFDUnknown), mPort(0), mRecvTm(0), mSendTm(0), 
mMakeTm(misc::GetTimeofday()), mSockType(type), mSockProto(proto), mSockFlag(flag) { }

wSocket::~wSocket() {
    Close();
}

const wStatus& wSocket::Close() {
    if (mFD != kFDUnknown && close(mFD) == -1) {
        return mStatus = wStatus::IOError("wSocket::Close failed", strerror(errno));
    }
    mFD = kFDUnknown;
    return mStatus.Clear();
}

const wStatus& wSocket::SetFL(bool nonblock) {
    if (fcntl(mFD, F_SETFL, (nonblock == true ? fcntl(mFD, F_GETFL, 0) | O_NONBLOCK : fcntl(mFD, F_GETFL, 0) & ~O_NONBLOCK)) == -1) {
        return mStatus = wStatus::IOError("wSocket::SetFL F_SETFL failed", strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
    mRecvTm = misc::GetTimeofday();
    
    while (true) {
        *size = recv(mFD, reinterpret_cast<void*>(buf), len, 0);
        if (*size > 0) {
            mStatus.Clear();
            break;
        } else if (*size == 0) {
        	mStatus = wStatus::IOError("wSocket::RecvBytes, client was closed", "", false);
            break;
        } else if (errno == EAGAIN) {
            mStatus.Clear();
            break;
        } else if (errno == EINTR) {
            // 操作被信号中断，中断后唤醒继续处理
            // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
            continue;
        } else {
            mStatus = wStatus::IOError("wSocket::RecvBytes, recv failed", strerror(errno));
            break;
        }
    }
    return mStatus;
}

const wStatus& wSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
    mSendTm = misc::GetTimeofday();
    ssize_t sendedlen = 0, leftlen = len;
    while (true) {
        *size = send(mFD, reinterpret_cast<void*>(buf + sendedlen), leftlen, 0);
        if (*size >= 0) {
            sendedlen += *size;
            if ((leftlen -= *size) == 0) {
                mStatus.Clear();
                *size = sendedlen;
                break;
            }
        } else if (errno == EAGAIN) {
            mStatus.Clear();
            break;
        } else if (errno == EINTR) {
            // 操作被信号中断，中断后唤醒继续处理
            // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
            continue;
        } else {
            mStatus = wStatus::IOError("wSocket::SendBytes, send failed", strerror(errno));
            break;
        }
    }
    return mStatus;
}

}   // namespace hnet
