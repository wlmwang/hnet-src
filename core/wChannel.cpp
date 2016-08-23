
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wChannel.h"
#include "wChannelCmd.h"

void wChannel::Close()
{
    if (mCh[0] == FD_UNKNOWN || close(mCh[0]) == -1) 
    {
        mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] close() channel failed:%s", strerror(mErr));
    }
    if (mCh[1] == FD_UNKNOWN || close(mCh[1]) == -1) 
    {
        mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] close() channel failed:%s", strerror(mErr));
    }
	mFD = FD_UNKNOWN;
}

int wChannel::Open()
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, mCh) == -1)
    {
        mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] socketpair failed:%s", strerror(mErr));
    	return -1;
    }
	
	//noblock
    if (fcntl(mCh[0], F_SETFL, fcntl(mCh[0], F_GETFL) | O_NONBLOCK) == -1)
    {
        mErr = errno;
    	LOG_ERROR(ELOG_KEY, "[system] fcntl(O_NONBLOCK) failed:%s", strerror(mErr));
    	Close();
    	return -1;
    }
    if (fcntl(mCh[1], F_SETFL, fcntl(mCh[1], F_GETFL) | O_NONBLOCK) == -1)
    {
        mErr = errno;
    	LOG_ERROR(ELOG_KEY, "[system] fcntl(O_NONBLOCK) failed:%s", strerror(mErr));
    	Close();
    	return -1;
    }
	
	//cloexec
    if (fcntl(mCh[0], F_SETFD, FD_CLOEXEC) == -1) 
    {
        mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] fcntl(FD_CLOEXEC) failed:%s", strerror(mErr));
    }
    if (fcntl(mCh[1], F_SETFD, FD_CLOEXEC) == -1) 
    {
        mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] fcntl(FD_CLOEXEC) failed:%s", strerror(mErr));
    }
	
	mFD = mCh[1];	//ch[1]被监听（可读事件）
    return mFD;
}

ssize_t wChannel::SendBytes(char *vArray, size_t vLen)
{
	mSendTime = GetTickCount();
	mFD = mCh[0];
	
    ssize_t	n;
	struct msghdr msg;
	
	/**
	 *  附属信息
	 *  发送文件描述符。 msghdr.msg_control 缓冲区必须与 cmsghdr 结构对齐
	 */
	struct ChannelReqCmd_s *pChannel = (struct ChannelReqCmd_s*) (vArray + sizeof(int));	//去除消息头（消息长度）
	union 
    {
        struct cmsghdr  cm;
        char	space[CMSG_SPACE(sizeof(int))];
    } cmsg;
	
    if (pChannel->mFD == -1) 
    {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    }
    else
    {
        msg.msg_control = (caddr_t) &cmsg;	//typedef void* caddr_t;
        msg.msg_controllen = sizeof(cmsg);

        memset(&cmsg, 0, sizeof(cmsg));

        cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));
        cmsg.cm.cmsg_level = SOL_SOCKET;
        cmsg.cm.cmsg_type = SCM_RIGHTS;	//附属数据对象是文件描述符

        memcpy(CMSG_DATA(&cmsg.cm), &pChannel->mFD, sizeof(int));
    }
	
	/**
	 *  套接口地址
	 *  当通道是数据报(UDP)套接口时需要， msg_name 指向要发送或是接收信息的套接口地址
	 *  注意：并不需要将套接口地址显示转换为 (struct sockaddr *) 
	 */
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
	
	/**
	 *  I/O向量引用，实际的数据缓冲区
	 *  注意：当要同步文件描述符，iov_base 至少一字节
	 */
	struct iovec iov[1];
    iov[0].iov_base = (char *) vArray;
    iov[0].iov_len = vLen;
	
    msg.msg_iov = iov;		//多io缓冲区的地址
    msg.msg_iovlen = 1;		//缓冲区的个数
	
	msg.msg_flags = 0;

    n = sendmsg(mFD, &msg, 0);

    if (n == -1) 
    {
        mErr = errno;
        if ((mErr = EINTR) || (mErr == EAGAIN)) 
        {
            return 0;
        }
		
		LOG_ERROR(ELOG_KEY, "[system] sendmsg() failed:%s", strerror(mErr));
        return -1;
    }

	LOG_DEBUG(ELOG_KEY, "[system] sendmsg() success, data len: %d, real send len %d", n, vLen);
    return n;
}

ssize_t wChannel::RecvBytes(char *vArray, size_t vLen)
{
	mRecvTime = GetTickCount();
	mFD = mCh[1];	//0负责接受数据FD
	
    ssize_t	n;
    struct iovec iov[1];
    struct msghdr msg;
	
    union 
    {
        struct cmsghdr  cm;
        char	space[CMSG_SPACE(sizeof(int))];
    } cmsg;

    iov[0].iov_base = (char *) vArray;
    iov[0].iov_len = vLen;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_control = (caddr_t) &cmsg;
    msg.msg_controllen = sizeof(cmsg);

    n = recvmsg(mFD, &msg, 0);

    if (n == -1)
    {
        mErr = errno;
        if (mErr == EAGAIN) 
        {
            return EAGAIN;
        }
		
		LOG_ERROR(ELOG_KEY, "[system] recvmsg() failed:%s", strerror(mErr));
        return -1;
    }

    if (n == 0) 
    {
		LOG_ERROR(ELOG_KEY, "[system] recvmsg() returned zero");
        return -1;
    }
    
    if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) 
    {
        LOG_ERROR(ELOG_KEY, "[system] recvmsg() truncated data");
    }

    //是否是打开fd文件描述符channel
    if (n == sizeof(struct ChannelReqOpen_t) + sizeof(int))
    {
        struct ChannelReqOpen_t *pChannel = (struct ChannelReqOpen_t*) (vArray + sizeof(int));        
        if (pChannel->GetCmd() == CMD_CHANNEL_REQ && pChannel->GetPara() == CHANNEL_REQ_OPEN)
        {
            if (cmsg.cm.cmsg_len < (socklen_t) CMSG_LEN(sizeof(int))) 
            {
                LOG_ERROR(ELOG_KEY, "[system] recvmsg() returned too small ancillary data");
                return -1;
            }

            if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS)
            {
                LOG_ERROR(ELOG_KEY, "[system] recvmsg() returned invalid ancillary data");
                return -1;
            }
            
            memcpy(&pChannel->mFD, CMSG_DATA(&cmsg.cm), sizeof(int));   //文件描述符
        }
    }
	
    return n;
}

int &wChannel::operator[](int i)
{
    if (i > 1)
    {
        LOG_ERROR(ELOG_KEY, "[system] leap the pale channel");
    }
    return mCh[i];
}
