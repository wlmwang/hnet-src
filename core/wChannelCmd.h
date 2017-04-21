
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

const uint8_t CMD_CHANNEL_REQ = 0;
struct wChannelReqCmd_s : public hnet::wCommand {
    int32_t mPid;
    wChannelReqCmd_s(uint8_t para) : wCommand(CMD_CHANNEL_REQ, para), mPid(-1) { }

    inline void set_pid(int32_t pid) {
        mPid = pid;
    }
    inline int32_t pid() {
        return mPid;
    }
};

const uint8_t CHANNEL_REQ_OPEN = 10;
struct wChannelReqOpen_t : public wChannelReqCmd_s {
    int32_t mSlot;
    int32_t mFd;
    wChannelReqOpen_t() : wChannelReqCmd_s(CHANNEL_REQ_OPEN), mSlot(-1), mFd(-1) { }

    inline void set_slot(int32_t slot) {
        mSlot = slot;
    }
    inline void set_fd(int32_t fd) {
        mFd = fd;
    }
    inline int32_t slot() {
        return mSlot;
    }
    inline int32_t fd() {
        return mFd;
    }
};

const uint8_t CHANNEL_REQ_CLOSE = 11;
struct wChannelReqClose_t : public wChannelReqCmd_s {
    int32_t mSlot;
    wChannelReqClose_t() : wChannelReqCmd_s(CHANNEL_REQ_CLOSE), mSlot(-1) { }

    inline void set_slot(int32_t slot) {
        mSlot = slot;
    }
    inline int32_t slot() {
        return mSlot;
    }
};

const uint8_t CHANNEL_REQ_QUIT = 12;
struct wChannelReqQuit_t : public wChannelReqCmd_s {
    wChannelReqQuit_t() : wChannelReqCmd_s(CHANNEL_REQ_QUIT) { }
};

const uint8_t CHANNEL_REQ_TERMINATE = 13;
struct wChannelReqTerminate_t : public wChannelReqCmd_s {
    wChannelReqTerminate_t() : wChannelReqCmd_s(CHANNEL_REQ_TERMINATE) { }
};

#pragma pack()

}	// namespace hnet

#endif