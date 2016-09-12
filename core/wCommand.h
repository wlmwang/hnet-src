
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_COMMAND_H_
#define _W_COMMAND_H_

#include "wCore.h"

namespace hnet {

const uint8_t kCmdNull = 0;
const uint8_t kParaNull = 0;

// 消息ID
inline uint16_t CmdId(uint8_t cmd, uint8_t para) {
    if (kLittleEndian) {
        return (static_cast<uint16_t>(para) << 8) | (static_cast<uint16_t>(cmd));
    } else {
        return (static_cast<uint16_t>(cmd) << 8) | (static_cast<uint16_t>(para));
    }
}

#pragma pack(1)

struct wNull_t {
    wNull_t(const uint8_t cmd, const uint8_t para): mCmd(cmd), mPara(para) { }

    uint16_t GetId() const {
        return mId;
    }
    uint8_t GetCmd() const {
        return mCmd;
    }
    uint8_t GetPara() const {
        return mPara;
    }
    
    union {
        struct {
            uint8_t mCmd; 
            uint8_t mPara;
        };
        uint16_t mId;
    };
};

struct wCommand : public wNull_t {
    wCommand(const uint8_t cmd = kCmdNull, const uint8_t para = kParaNull): wNull_t(cmd, para) { }
};

#pragma pack()

}   // namespace hnet

#endif