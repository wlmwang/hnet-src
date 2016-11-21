
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_PING_H_
#define _W_PING_H_

#include <sys/socket.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

namespace {

const int32_t   kPacketSize = 4096;
const int32_t   kDataSize = 56;
const int8_t    kRetryTimes = 4;
const int8_t    kIcmpData = 4;

}   // namespace hnet

class wPing : private wNoncopyable {
public:
    wPing();
    ~wPing();
    const wStatus& Open();
    const wStatus& Close();
    const wStatus& SetTimeout(float timeout = 0.1);
    const wStatus& Ping(const char *ip);

protected:
    const wStatus& SetSendTimeout(float timeout = 0.1);
    const wStatus& SetRecvTimeout(float timeout = 0.1);
    const wStatus& SendPacket();
    const wStatus& RecvPacket();
    int Pack();
    int Unpack(char *buf, int len);
    unsigned short CalChksum(unsigned short *addr, int len);

    int mFD;
    int mSeqNum;
    pid_t mPid;

    std::string mStrIp;
    struct sockaddr_in mDestAddr;	//目的地址
    struct sockaddr_in mFromAddr;	//返回地址

    char mSendpacket[kPacketSize];
    char mRecvpacket[kPacketSize];
    char mCtlpacket[kPacketSize];
    wStatus mStatus;
};

}   // namespace hnet

#endif
