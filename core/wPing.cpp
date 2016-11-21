
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include "wPing.h"
#include "wMisc.h"

namespace hnet {

wPing::wPing() : mFD(kFDUnknown), mSeqNum(0), mPid(getpid()) { }

wPing::~wPing() {
    Close();
}

const wStatus& wPing::Close() {
    if (close(mFD) != 0) {
        return mStatus = wStatus::IOError("wPing::Close, close failed", strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wPing::Open() {
    if ((mFD = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        return mStatus = wStatus::AccessIllegal("wPing::open failed, only root access", strerror(errno));
    }

    // 扩大套接字接收缓冲区到50k，主要为了减小接收缓冲区溢出的的可能性
    // 若无意中ping一个广播地址或多播地址，将会引来大量应答
    const int size = 50*1024;
    if (setsockopt(mFD, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) != 0) {
        return mStatus = wStatus::AccessIllegal("wPing::open, set socket receive buf failed", strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wPing::SetTimeout(float timeout) {
    mStatus = SetSendTimeout(timeout);
    if (!mStatus.Ok()) {
        return mStatus;
    }
    return SetRecvTimeout(timeout);
}

const wStatus& wPing::SetSendTimeout(float timeout) {
    struct timeval tv;
    tv.tv_sec = timeout>=0 ? static_cast<int>(timeout) : 0;
    tv.tv_usec = static_cast<int>((timeout - static_cast<int>(timeout)) * 1000000);
    if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
    	tv.tv_sec = 0;
        tv.tv_usec = 700000;
    }
    if (setsockopt(mFD, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        return mStatus = wStatus::IOError("wPing::SetSendTimeout, setsockopt SO_SNDTIMEO failed", strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wPing::SetRecvTimeout(float timeout) {
    struct timeval tv;
    tv.tv_sec = timeout>=0 ? static_cast<int>(timeout) : 0;
    tv.tv_usec = static_cast<int>((timeout - static_cast<int>(timeout)) * 1000000);
    if (tv.tv_usec < 0 || tv.tv_usec >= 1000000 || (tv.tv_sec == 0 && tv.tv_usec == 0)) {
    	tv.tv_sec = 0;
        tv.tv_usec = 700000;
    }
    if (setsockopt(mFD, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        return mStatus = wStatus::IOError("wPing::SetRecvTimeout, setsockopt SO_RCVTIMEO failed", strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wPing::Ping(const char *ip) {
    if (ip == NULL || mFD == kFDUnknown) {
        return mStatus = wStatus::InvalidArgument("wPing::Ping failed", "ip is NULL or open was not called");
    }

    mStrIp = ip;
    bzero(&mDestAddr, sizeof(mDestAddr));
    mDestAddr.sin_family = AF_INET;
    unsigned long inaddr = 0l;
    if ((inaddr = inet_addr(mStrIp.c_str())) == INADDR_NONE) {
        /*
        struct hostent *host;
        if ((host = gethostbyname(mStrIp.c_str())) == NULL) {
            return -1;
        }
        memcpy((char *)&mDestAddr.sin_addr, host->h_addr, host->h_length);
        */
        return mStatus = wStatus::IOError("wPing::Ping, inet_addr failed, ip: " + std::string(ip), strerror(errno));
    } else {
        mDestAddr.sin_addr.s_addr = inaddr;
    }

    // 发送所有ICMP报文
    mStatus = SendPacket();
    if (!mStatus.Ok()) {
        return mStatus;
    }
    return RecvPacket();
}

// 发送num个ICMP报文
const wStatus& wPing::SendPacket() {
    // 设置ICMP报头
    int len = Pack();

    if (sendto(mFD, mSendpacket, len, 0, reinterpret_cast<struct sockaddr *>(&mDestAddr), sizeof(mDestAddr)) < len) {
        return mStatus = wStatus::IOError("wPing::Ping, sendto ping error", strerror(errno));
    }
    return mStatus.Clear();
}

// 接收所有ICMP报文
const wStatus& wPing::RecvPacket() {
    struct msghdr msg;
    struct iovec iov;
    memset(&msg, 0, sizeof(msg));
    memset(&iov, 0, sizeof(iov));
    memset(mRecvpacket, 0, sizeof(mRecvpacket));
    memset(mCtlpacket, 0, sizeof(mCtlpacket));
    
    int len = 0, i = 0;
    for (i = 0; i < kRetryTimes; i++) {
        iov.iov_base = mRecvpacket;
        iov.iov_len = sizeof(mRecvpacket);
        msg.msg_name = reinterpret_cast<struct sockaddr *>(&mFromAddr);
        
        msg.msg_namelen = sizeof(struct sockaddr_in);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = mCtlpacket;
        msg.msg_controllen = sizeof(mCtlpacket);

        len = recvmsg(mFD, &msg, 0);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            } else if(errno == EAGAIN) {
                continue;
                wStatus::IOError("wPing::Ping, ping recvmsg timeout, ip: " + mStrIp, strerror(errno));
            } else {
                return mStatus = wStatus::IOError("wPing::Ping, ping recvmsg error, ip: " + mStrIp, strerror(errno));
            }
        } else if (len == 0) {
            return mStatus = wStatus::IOError("wPing::Ping, ping recvmsg return 0, ip: ", mStrIp);
        } else {
            struct ip *iphdr = reinterpret_cast<struct ip *>(mRecvpacket);
            if (iphdr->ip_p == IPPROTO_ICMP && iphdr->ip_src.s_addr == mDestAddr.sin_addr.s_addr) {
                break;
            }
        }
    }

    if (i >= kRetryTimes) {
        return mStatus = wStatus::IOError("wPing::Ping, ping recvmsg retry over times, ip: ", mStrIp);
    }
    if (Unpack(mRecvpacket, len) < 0) {
        return mStatus = wStatus::Corruption("wPing::Ping, parse error, ip: ", mStrIp);
    }
    
    return mStatus.Clear();
}

// 设置ICMP请求报头
int wPing::Pack() {
    memset(mSendpacket, 0, sizeof(mSendpacket));
    struct icmp *icmp = reinterpret_cast<struct icmp*>(mSendpacket);

    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = mPid;
    icmp->icmp_seq = mSeqNum++;
    memset(icmp->icmp_data, 0xa5, kDataSize);
    icmp->icmp_data[0] = kIcmpData;

    // 校验算法
    int len = 8 + kDataSize;
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = CalChksum(reinterpret_cast<unsigned short *>(icmp), len);
    return len;
}

// 剥去ICMP报头
int wPing::Unpack(char buf[], int len) {
    if (len == 0) {
        return -7;
    }
    if (buf == NULL) {
        return -8;
    }

    struct ip *ip = reinterpret_cast<struct ip *>(buf);
    if (ip->ip_p != IPPROTO_ICMP) {
        return -2;
    }
	
    // 越过ip报头,指向ICMP报头
    int iphdrlen = ip->ip_hl << 2;
    struct icmp *icmp = reinterpret_cast<struct icmp *>(buf + iphdrlen);

    // ICMP报头及ICMP数据报的总长度
    len -= iphdrlen;
    if (len < 8) {
        return -3;
    }

    // 确保所接收的是我所发的的ICMP的回应
    if (icmp->icmp_type == ICMP_ECHOREPLY) {
        if (icmp->icmp_id != mPid) {
            return -4;
        }
        if(len < 16) {
            return -5;
        }
        if (icmp->icmp_data[0] != kIcmpData) {
            return -6;
        }
    } else {
        return -1;
    }
    return 0;
}

// 校验和
unsigned short wPing::CalChksum(unsigned short *addr, int len) {
    int sum = 0;
    int nleft = len;
    unsigned short *w = addr;
    // 把ICMP报头二进制数据以2字节为单位累加起来
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }
    
    // 若ICMP报头为奇数个字节，会剩下最后一字节。把最后一个字节视为一个2字节数据的高字节，这个2字节数据的低字节为0，继续累加
    unsigned short answer = 0;
    if (nleft == 1) {
        *(reinterpret_cast<unsigned char *>(&answer)) = *(reinterpret_cast<unsigned char *>(w));
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

}   // namespace hnet
