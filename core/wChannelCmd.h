
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CHANNEL_CMD_H_
#define _W_CHANNEL_CMD_H_

#include "wCore.h"
#include "wCommand.h"

namespace hnet {

#pragma pack(1)

//++++++++++++请求数据结构
const uint8_t CMD_CHANNEL_REQ = 1;
struct ChannelReqCmd_s : public wCommand {
    ChannelReqCmd_s(uint8_t para) : wCommand(CMD_CHANNEL_REQ, para), mPid(0), mSlot(0), mFD(kFDUnknown) { }
    
    int32_t mPid;	// 发送方进程pid
    int32_t mSlot;	// 发送方进程表中偏移量(下标)
    int32_t mFD;	// 发送方ch[0]描述符
};

const uint8_t CHANNEL_REQ_OPEN = 0;
struct ChannelReqOpen_t : public ChannelReqCmd_s {
    ChannelReqOpen_t() : ChannelReqCmd_s(CHANNEL_REQ_OPEN) { }
};

const uint8_t CHANNEL_REQ_CLOSE = 1;
struct ChannelReqClose_t : public ChannelReqCmd_s {
    ChannelReqClose_t() : ChannelReqCmd_s(CHANNEL_REQ_CLOSE) { }
};

const uint8_t CHANNEL_REQ_QUIT = 2;
struct ChannelReqQuit_t : public ChannelReqCmd_s {
    ChannelReqQuit_t() : ChannelReqCmd_s(CHANNEL_REQ_QUIT) { }
};

const uint8_t CHANNEL_REQ_TERMINATE = 3;
struct ChannelReqTerminate_t : public ChannelReqCmd_s {
    ChannelReqTerminate_t() : ChannelReqCmd_s(CHANNEL_REQ_TERMINATE) { }
};

const uint8_t CHANNEL_REQ_REOPEN = 4;
struct ChannelReqReopen_t : public ChannelReqCmd_s {
    ChannelReqReopen_t() : ChannelReqCmd_s(CHANNEL_REQ_REOPEN) { }
};

#pragma pack()

}   // namespace hnet

#endif