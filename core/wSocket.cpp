
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wSocket.h"

namespace hnet {

wSocket::wSocket(SockType type, SockProto proto, SockFlag flag) : mFD(kFDUnknown), mPort(0), mRecvTm(0), mSendTm(0), 
mMakeTm(soft::TimeUsec()), mSockType(type), mSockProto(proto), mSockFlag(flag) { }

wSocket::~wSocket() {
    Close();
}

int wSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
    mRecvTm = soft::TimeUsec();

    int ret = 0;
    while (true) {
        *size = recv(mFD, reinterpret_cast<void*>(buf), len, 0);

        if (*size > 0) {
            break;
        } else if (*size == 0) {    // FIN package // client was closed
            ret = -1;
            break;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {   // Resource temporarily unavailable // 资源暂时不够(可能读缓冲区没有数据)
            ret = 0;
            break;
        } else if (errno == EINTR) {    // Interrupted system call
            continue;
            //ret = 0;
            //break;
        } else {
            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::RecvBytes recv() failed", error::Strerror(errno).c_str());
            ret = -1;
            break;
        }
    }
    return ret;
}

int wSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
    mSendTm = soft::TimeUsec();

    int ret = 0;
    ssize_t sendedlen = 0, leftlen = len;
    while (leftlen > 0) {
        *size = send(mFD, reinterpret_cast<void*>(buf + sendedlen), leftlen, 0);

        if (*size >= 0) {
            sendedlen += *size;
            if ((leftlen -= *size) == 0) {
                *size = sendedlen;
                break;
            }
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {   // Resource temporarily unavailable // 资源暂时不够(可能写缓冲区满)
            ret = 0;
            break;
        } else if (errno == EINTR) {    // Interrupted system call
            continue;
            //ret = 0;
            //break;
        } else if (errno == EPIPE) {    // RST package // client was closed
            ret = -1;
            break;
        } else {
            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::SendBytes send() failed", error::Strerror(errno).c_str());
            ret = -1;
            break;
        }
    }
    return ret;
}

int wSocket::Close() {
    int ret = close(mFD);
    if (ret == -1) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::Close close() failed", error::Strerror(errno).c_str());
    }
    mFD = kFDUnknown;
    return ret;
}

int wSocket::GetNonblock() {
    int ret = fcntl(mFD, F_GETFL, 0);
    if (ret == -1) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::GetNonblock fcntl() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return ret & O_NONBLOCK;
}

int wSocket::SetNonblock(bool nonblock) {
    int ret = fcntl(mFD, F_SETFL, (nonblock==true? fcntl(mFD, F_GETFL, 0) | O_NONBLOCK : fcntl(mFD, F_GETFL, 0) & ~O_NONBLOCK));
    if (ret == -1) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wSocket::SetNonblock fcntl() O_NONBLOCK failed", error::Strerror(errno).c_str());
    }
    return ret;
}

}   // namespace hnet
