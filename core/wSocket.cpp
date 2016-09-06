
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSocket.h"
#include "wLog.h"
#include "wMisc.h"

namespace hnet {

wSocket::wSocket(SockType type, SockProto proto, SockFlag flag) : mFD(kFDUnknown), mPort(0), mRecvTm(0), mSendTm(0), 
mMakeTm(misc::GetTimeofday()), mSockType(type), mSockProto(proto), mSockFlag(flag) { }

wSocket::~wSocket() {
    Close();
}

wStatus wSocket::Close() {
    if (close(mFD) == -1) {
        return mStatus = wStatus::IOError("wSocket::Close failed", strerror(errno));
    }
    mFD = kFDUnknown;
    return mStatus = wStatus::Nothing();
}

wStatus wSocket::SetFL(bool nonblock) {
    int flags = fcntl(mFD, F_GETFL, 0);
    if (flags == -1) {
        return mStatus = wStatus::IOError("wSocket::SetFL F_GETFL failed", strerror(errno));
    }
    if (fcntl(mFD, F_SETFL, (nonblock == true ? flags | O_NONBLOCK : flags & ~O_NONBLOCK)) == -1) {
        return mStatus = wStatus::IOError("wSocket::SetFL F_SETFL failed", strerror(errno));
    }
    return mStatus = wStatus::Nothing();
}

ssize_t wSocket::RecvBytes(char *vArray, size_t vLen) {
    mRecvTm = misc::GetTimeofday();

    while (true) {
        ssize_t recvlen = recv(mFD, vArray, vLen, 0);
        if (recvlen > 0) {
            return recvlen;
        } else if (recvlen == 0) {
            return kSeClosed;	// FIN，对端关闭
        } else {
            if (errno == EINTR) {
                continue;
            }

            // 返回0，表示当前接受缓冲区已满，可以继续接受，但需等待下一轮epoll（水平触发）通知
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }

            mStatus = wStatus::IOError("wSocket::RecvBytes, recv failed", strerror(errno));
            return recvlen;
        }
    }
}

ssize_t wSocket::SendBytes(char *vArray, size_t vLen) {
    mSendTm = misc::GetTimeofday();

    size_t iLeftLen = vLen;
    size_t iHaveSendLen = 0;
    while (true) {
        ssize_t iSendLen = send(mFD, vArray + iHaveSendLen, iLeftLen, 0);
        if (iSendLen >= 0) {
            iLeftLen -= iSendLen;
            iHaveSendLen += iSendLen;
            if (iLeftLen == 0) {
                return vLen;
            }
        } else {
            if (errno == EINTR) {
                continue;
            }

            // 返回0，表示当前发送缓冲区已满，可以继续发送，但需等待下一轮epoll（水平触发）通知
            if (iSendLen < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return 0;
            }

            mStatus = wStatus::IOError("wSocket::SendBytes, send failed", strerror(errno));
            return iSendLen;
        }
    }
}

}   // namespace hnet
