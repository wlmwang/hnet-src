
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CHANNEL_CMD_H_
#define _W_CHANNEL_CMD_H_

#include "wCore.h"
#include "wCommand.h"

namespace hnet {

const uint8_t CMD_CHANNEL_REQ = 1;
struct wChannelReqCmd_s : public hnet::wCommand {
    int32_t mPid;
    wChannelReqCmd_s(uint8_t para) : wCommand(CMD_CHANNEL_REQ, para), mPid(-1) { }

    inline void set_pid(int32_t pid) {
        mPid = pid;
    }
    inline int32_t pid() {
        return mPid;
    }
    inline void ParseFromArray(char* buf, uint32_t len) {
        wChannelReqCmd_s* w = reinterpret_cast<wChannelReqCmd_s*>(buf);
        set_pid(w->pid());
    }
};

const uint8_t CHANNEL_REQ_OPEN = 0;
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
    inline void ParseFromArray(char* buf, uint32_t len) {
        wChannelReqCmd_s::ParseFromArray(buf, len);

        wChannelReqOpen_t* w = reinterpret_cast<wChannelReqOpen_t*>(buf);
        set_slot(w->slot());
        set_fd(w->fd());
    }
};

const uint8_t CHANNEL_REQ_CLOSE = 1;
struct wChannelReqClose_t : public wChannelReqCmd_s {
    int32_t mSlot;
    wChannelReqClose_t() : wChannelReqCmd_s(CHANNEL_REQ_CLOSE), mSlot(-1) { }

    inline void set_slot(int32_t slot) {
        mSlot = slot;
    }
    inline int32_t slot() {
        return mSlot;
    }
    inline void ParseFromArray(char* buf, uint32_t len) {
        wChannelReqCmd_s::ParseFromArray(buf, len);

        wChannelReqClose_t* w = reinterpret_cast<wChannelReqClose_t*>(buf);
        set_slot(w->slot());
    }
};

const uint8_t CHANNEL_REQ_QUIT = 2;
struct wChannelReqQuit_t : public wChannelReqCmd_s {
    wChannelReqQuit_t() : wChannelReqCmd_s(CHANNEL_REQ_QUIT) { }
};

const uint8_t CHANNEL_REQ_TERMINATE = 3;
struct wChannelReqTerminate_t : public wChannelReqCmd_s {
    wChannelReqTerminate_t() : wChannelReqCmd_s(CHANNEL_REQ_TERMINATE) { }
};

}	// namespace hnet

#endif