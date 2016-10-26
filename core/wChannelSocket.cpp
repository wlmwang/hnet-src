
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/un.h>
#include <sys/uio.h>
#include "wChannelSocket.h"
#include "wChannelCmd.h"
#include "wMisc.h"

namespace hnet {

wChannelSocket::~wChannelSocket() {
    Close();
}

wStatus wChannelSocket::Open() {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, mChannel) == -1) {
        return mStatus = wStatus::IOError("wChannelSocket::Open socketpair() AF_UNIX failed", strerror(errno));
    }
    
    if (fcntl(mChannel[0], F_SETFL, fcntl(mChannel[0], F_GETFL) | O_NONBLOCK) == -1) {
        return mStatus = wStatus::IOError("wChannelSocket::Open [0] fcntl() O_NONBLOCK failed", strerror(errno));
    } else if (fcntl(mChannel[1], F_SETFL, fcntl(mChannel[1], F_GETFL) | O_NONBLOCK) == -1) {
        return mStatus = wStatus::IOError("wChannelSocket::Open [1] fcntl() O_NONBLOCK failed", strerror(errno));
    }
    
    // 不返回错误 todo
    if (fcntl(mChannel[0], F_SETFD, FD_CLOEXEC) == -1) {
        // mStatus = wStatus::IOError("wChannelSocket::Open [0] fcntl() FD_CLOEXEC failed", strerror(errno));
    } else if (fcntl(mChannel[1], F_SETFD, FD_CLOEXEC) == -1) {
        // mStatus = wStatus::IOError("wChannelSocket::Open [1] fcntl() FD_CLOEXEC failed", strerror(errno));
    }
    // mChannel[1]被监听（可读事件）
    mFD = mChannel[1];
    return mStatus = wStatus::Nothing();
}

wStatus wChannelSocket::Close() {
    if (close(mChannel[0]) == -1) {
        return mStatus = wStatus::IOError("wChannelSocket::Close [0] failed", strerror(errno));
    } else if (close(mChannel[1]) == -1) {
        return mStatus = wStatus::IOError("wChannelSocket::Close [1] failed", strerror(errno));
    }
    mFD = kFDUnknown;
    return mStatus = wStatus::Nothing();
}

wStatus wChannelSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
    mFD = mChannel[0];
    mSendTm = misc::GetTimeofday();
    
    // 去除消息头
    struct ChannelReqCmd_s *channel = reinterpret_cast<struct ChannelReqCmd_s*>(buf + sizeof(uint32_t));
    // msghdr.msg_control 缓冲区必须与 cmsghdr 结构对齐
    union {
        struct cmsghdr  cm;
        char space[CMSG_SPACE(sizeof(int))];
    } cmsg;
    
    // 附属信息，一般为同步进程间文件描述符
    struct msghdr msg;
    if (channel->mFD == kFDUnknown) {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    } else {
        msg.msg_control = reinterpret_cast<caddr_t>(&cmsg);  //typedef void* caddr_t;
        msg.msg_controllen = sizeof(cmsg);
        memset(&cmsg, 0, sizeof(cmsg));
        cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));
        cmsg.cm.cmsg_level = SOL_SOCKET;

        // 附属数据对象是文件描述符
        cmsg.cm.cmsg_type = SCM_RIGHTS;

        memcpy(CMSG_DATA(&cmsg.cm), &channel->mFD, sizeof(int));
    }
    
    // 套接口地址，msg_name指向要发送或是接收信息的套接口地址，仅当是数据包UDP是才需要
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    
    // 实际的数据缓冲区，I/O向量引用。当要同步文件描述符，iov_base 至少一字节
    struct iovec iov[1];
    iov[0].iov_base = reinterpret_cast<char *>(buf);
    iov[0].iov_len = len;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;

    *size = sendmsg(mFD, &msg, 0);
    if (*size >= 0) {
        mStatus = wStatus::Nothing();
    } else if (*size == -1 && (errno == EINTR || errno == EAGAIN)) {
        mStatus = wStatus::Nothing();
    } else {
        mStatus = wStatus::IOError("wChannelSocket::SendBytes, sendmsg failed", strerror(errno));
    }
    return mStatus;
}

wStatus wChannelSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
    mFD = mChannel[1];
    mRecvTm = misc::GetTimeofday();

    // msghdr.msg_control 缓冲区必须与 cmsghdr 结构对齐
    union {
        struct cmsghdr  cm;
        char space[CMSG_SPACE(sizeof(int))];
    } cmsg;

    // 实际的数据缓冲区，I/O向量引用。当要同步文件描述符，iov_base 至少一字节
    struct iovec iov[1];
    iov[0].iov_base = reinterpret_cast<char *>(buf);
    iov[0].iov_len = len;

    // 附属信息，一般为同步进程间文件描述符
    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = reinterpret_cast<caddr_t>(&cmsg);  //typedef void* caddr_t;
    msg.msg_controllen = sizeof(cmsg);

    *size = recvmsg(mFD, &msg, 0);
    if (*size == 0) {
        mStatus = wStatus::IOError("wChannelSocket::RecvBytes, client was closed", "");
    } else if (*size == -1 && (errno == EINTR || errno == EAGAIN)) {
        mStatus = wStatus::Nothing();
    } else if (*size == -1) {
        mStatus = wStatus::IOError("wChannelSocket::RecvBytes, recvmsg failed", strerror(errno));
    } else {
        if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
            // [system] recvmsg() truncated data
        }
        // 是否是同步 打开fd文件描述符 的channel
        if (*size == sizeof(struct ChannelReqOpen_t) + sizeof(uint32_t)) {
            struct ChannelReqOpen_t *channel = reinterpret_cast<struct ChannelReqOpen_t*> (buf + sizeof(uint32_t));
            if (channel->GetCmd() == CMD_CHANNEL_REQ && channel->GetPara() == CHANNEL_REQ_OPEN) {
                if (cmsg.cm.cmsg_len < static_cast<socklen_t>(CMSG_LEN(sizeof(int)))) {
                    mStatus = wStatus::IOError("wChannelSocket::RecvBytes, recvmsg failed", "returned too small ancillary data");
                } else if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS) {
                    mStatus = wStatus::IOError("wChannelSocket::RecvBytes, recvmsg failed", "returned invalid ancillary data");
                } else {
                    // 文件描述符
                    memcpy(&channel->mFD, CMSG_DATA(&cmsg.cm), sizeof(int));
                }
            }
        }
    }
    return mStatus;
}

}   // namespace hnet
