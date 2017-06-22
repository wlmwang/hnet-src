
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSocket.h"
#include "wMisc.h"

namespace hnet {

wSocket::wSocket(SockType type, SockProto proto, SockFlag flag) : mFD(kFDUnknown), mPort(0), mRecvTm(0), mSendTm(0), 
mMakeTm(soft::TimeUsec()), mSockType(type), mSockProto(proto), mSockFlag(flag) { }

wSocket::~wSocket() {
    Close();
}

const wStatus& wSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
    mRecvTm = soft::TimeUsec();
    while (true) {
        *size = recv(mFD, reinterpret_cast<void*>(buf), len, 0);
        if (*size > 0) {
            break;
        } else if (*size == 0) {
        	mStatus = wStatus::IOError("wSocket::RecvBytes, client was closed", "", false);
            break;
        } else if (errno == EAGAIN) {
            break;
        } else if (errno == EINTR) {
            // 操作被信号中断，中断后唤醒继续处理
            // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
            continue;
        } else {
            mStatus = wStatus::IOError("wSocket::RecvBytes, recv failed", error::Strerror(errno));
            break;
        }
    }
    return mStatus;
}

const wStatus& wSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
    mSendTm = soft::TimeUsec();
    ssize_t sendedlen = 0, leftlen = len;
    while (true) {
        *size = send(mFD, reinterpret_cast<void*>(buf + sendedlen), leftlen, 0);
        if (*size >= 0) {
            sendedlen += *size;
            if ((leftlen -= *size) == 0) {
                *size = sendedlen;
                break;
            }
        } else if (errno == EAGAIN) {
            break;
        } else if (errno == EINTR) {
            // 操作被信号中断，中断后唤醒继续处理
            // 注意：系统中信号安装需提供参数SA_RESTART，否则请按 EAGAIN 信号处理
            continue;
        } else {
            mStatus = wStatus::IOError("wSocket::SendBytes, send failed", error::Strerror(errno));
            break;
        }
    }
    return mStatus;
}

int wSocket::Close() {
    int ret = close(mFD);
    if (ret == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::Close close() failed", error::Strerror(errno).c_str());
    }
    mFD = kFDUnknown;
    return ret;
}

int wSocket::GetNonblock() {
    int ret = fcntl(mFD, F_GETFL, 0);
    if (ret == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::GetNonblock fcntl() failed", error::Strerror(errno).c_str());
    }
    return ret & O_NONBLOCK;
}

int wSocket::SetNonblock(bool nonblock) {
    int ret = fcntl(mFD, F_SETFL, (nonblock==true? fcntl(mFD, F_GETFL, 0) | O_NONBLOCK : fcntl(mFD, F_GETFL, 0) & ~O_NONBLOCK));
    if (ret == -1) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::SetNonblock fcntl() O_NONBLOCK failed", error::Strerror(errno).c_str());
    }
    return ret;
}

}   // namespace hnet
