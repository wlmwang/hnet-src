
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/un.h>
#include <sys/uio.h>
#include "wLogger.h"
#include "wMisc.h"
#include "wTask.h"
#include "wChannelSocket.h"
#include "wChannel.pb.h"

namespace hnet {

wChannelSocket::~wChannelSocket() {
    Close();
}

const wStatus& wChannelSocket::Open() {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, mChannel) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        return mStatus = wStatus::IOError("wChannelSocket::Open socketpair() AF_UNIX failed", err);
    }
    
    if (fcntl(mChannel[0], F_SETFL, fcntl(mChannel[0], F_GETFL) | O_NONBLOCK) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        return mStatus = wStatus::IOError("wChannelSocket::Open [0] fcntl() O_NONBLOCK failed", err);
    } else if (fcntl(mChannel[1], F_SETFL, fcntl(mChannel[1], F_GETFL) | O_NONBLOCK) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        return mStatus = wStatus::IOError("wChannelSocket::Open [1] fcntl() O_NONBLOCK failed", err);
    }
    
    if (fcntl(mChannel[0], F_SETFD, FD_CLOEXEC) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
    	LOG_DEBUG(kLogPath, "%s : %s", "wChannelSocket::Open [0] fcntl() FD_CLOEXEC failed", err);
    } else if (fcntl(mChannel[1], F_SETFD, FD_CLOEXEC) == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
    	LOG_DEBUG(kLogPath, "%s : %s", "wChannelSocket::Open [1] fcntl() FD_CLOEXEC failed", err);
    }

    // mChannel[1]被监听（可读事件）
    mFD = mChannel[1];
    return mStatus.Clear();
}

const wStatus& wChannelSocket::Close() {
	close(mChannel[0]);
	close(mChannel[1]);
	mFD = kFDUnknown;
    return mStatus;
}

const wStatus& wChannelSocket::SendBytes(char buf[], size_t len, ssize_t *size) {
    mSendTm = misc::GetTimeofday();
    
    // msghdr.msg_control 缓冲区必须与 cmsghdr 结构对齐
    union {
        struct cmsghdr  cm;
        char space[CMSG_SPACE(sizeof(int32_t))];
    } cmsg;
    
    struct msghdr msg;

    // 数据协议
    uint8_t sp = static_cast<uint8_t>(coding::DecodeFixed8(buf + sizeof(uint32_t)));
    if (sp == kMpProtobuf) {
		uint16_t l = coding::DecodeFixed16(buf + sizeof(uint32_t) + sizeof(uint8_t));
		std::string name(buf + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t), l);
		if (name == "hnet.wChannelOpen") {
			// 附属信息
			wChannelOpen open;
			open.ParseFromArray(buf + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t) + l, len - sizeof(uint32_t) - sizeof(uint8_t) - sizeof(uint16_t) - l);

            msg.msg_control = reinterpret_cast<caddr_t>(&cmsg);
            msg.msg_controllen = sizeof(cmsg);
            memset(&cmsg, 0, sizeof(cmsg));

            cmsg.cm.cmsg_level = SOL_SOCKET;
            cmsg.cm.cmsg_type = SCM_RIGHTS;	// 附属数据对象是文件描述符
            cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int32_t));

            // 文件描述符
            *(int32_t *) CMSG_DATA(&cmsg.cm) = open.fd();

		} else {
	        msg.msg_control = NULL;
	        msg.msg_controllen = 0;
		}
    } else if (sp == kMpCommand) {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    }
    
    // 套接口地址，msg_name指向要发送或是接收信息的套接口地址，仅当是数据包UDP是才需要
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    
    // 实际的数据缓冲区，I/O向量引用。当要同步文件描述符，iov_base 至少一字节
    struct iovec iov[1];
    iov[0].iov_base = reinterpret_cast<char*>(buf);
    iov[0].iov_len = len;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;

    *size = sendmsg(mChannel[0], &msg, 0);
    if (*size >= 0) {
        mStatus.Clear();
    } else if (*size == -1 && (errno == EINTR || errno == EAGAIN)) {
        mStatus.Clear();
    } else {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        mStatus = wStatus::IOError("wChannelSocket::SendBytes, sendmsg failed", err);
    }
    return mStatus;
}

const wStatus& wChannelSocket::RecvBytes(char buf[], size_t len, ssize_t *size) {
    mRecvTm = misc::GetTimeofday();

    // msghdr.msg_control 缓冲区必须与 cmsghdr 结构对齐
    union {
        struct cmsghdr  cm;
        char space[CMSG_SPACE(sizeof(int32_t))];
    } cmsg;

    // 实际的数据缓冲区，I/O向量引用。当要同步文件描述符，iov_base 至少一字节
    struct iovec iov[1];
    iov[0].iov_base = reinterpret_cast<char*>(buf);
    iov[0].iov_len = len;

    // 附属信息，一般为同步进程间文件描述符
    struct msghdr msg;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = reinterpret_cast<caddr_t>(&cmsg);
    msg.msg_controllen = sizeof(cmsg);

    *size = recvmsg(mChannel[1], &msg, 0);
    if (*size == 0) {
        mStatus = wStatus::IOError("wChannelSocket::RecvBytes, client was closed", "");
    } else if (*size == -1 && (errno == EINTR || errno == EAGAIN)) {
        mStatus.Clear();
    } else if (*size == -1) {
    	char err[kMaxErrorLen];
    	::strerror_r(errno, err, kMaxErrorLen);
        mStatus = wStatus::IOError("wChannelSocket::RecvBytes, recvmsg failed", err);
    } else {
        if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
        	wStatus::IOError("wChannelSocket::RecvBytes, recvmsg() truncated data", "");
        }

        // 数据协议
        uint8_t sp = static_cast<uint8_t>(coding::DecodeFixed8(buf + sizeof(uint32_t)));
        if (sp == kMpProtobuf) {
            uint16_t l = coding::DecodeFixed16(buf + sizeof(uint32_t) + sizeof(uint8_t));
    		std::string name(buf + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t), l);
    		if (name == "hnet.wChannelOpen") {
    			if (cmsg.cm.cmsg_len < static_cast<socklen_t>(CMSG_LEN(sizeof(int32_t)))) {
                    mStatus = wStatus::IOError("wChannelSocket::RecvBytes, recvmsg failed", "returned too small ancillary data");
                } else if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS) {
                    mStatus = wStatus::IOError("wChannelSocket::RecvBytes, recvmsg failed", "returned invalid ancillary data");
                } else {
                	// 文件描述符
        			wChannelOpen open;
        			open.ParseFromArray(buf + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t) + l, *size - sizeof(uint32_t) - sizeof(uint8_t) - sizeof(uint16_t) - l);
                	int32_t fd = *(int32_t *) CMSG_DATA(&cmsg.cm);
                	open.set_fd(fd);
                	wTask::Assertbuf(buf, &open);
                }
    		}
        }
    }
    return mStatus;
}

}   // namespace hnet
