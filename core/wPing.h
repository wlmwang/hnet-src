
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
    wStatus Open();
    wStatus Close();
    wStatus SetTimeout(float fTimeout = 0.1 /** 秒 */);
    wStatus Ping(const char *ip);

protected:
    wStatus SetSendTimeout(float fTimeout = 0.1);
    wStatus SetRecvTimeout(float fTimeout = 0.1);
    wStatus SendPacket();
    wStatus RecvPacket();
    int Pack();
    int Unpack(char *buf, int len);
    unsigned short CalChksum(unsigned short *addr, int len);

    wStatus mStatus;
    int mFD;
    int mSeqNum;
    pid_t mPid;

    string mStrIp;
    struct sockaddr_in mDestAddr;	//目的地址
    struct sockaddr_in mFromAddr;	//返回地址

    char mSendpacket[kPacketSize];
    char mRecvpacket[kPacketSize];
    char mCtlpacket[kPacketSize];	
};

}   // namespace hnet

#endif
