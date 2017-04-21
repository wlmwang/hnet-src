
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wCommand.h"

#ifndef _EXAMPLE_CMD_H_
#define _EXAMPLE_CMD_H_

namespace example {

#pragma pack(1)

const uint8_t CMD_EXAMPLE_REQ = 50;
struct ExampleReqCmd_s : public hnet::wCommand {
    ExampleReqCmd_s(uint8_t para) : wCommand(CMD_EXAMPLE_REQ, para) { }
};

const uint8_t EXAMPLE_REQ_ECHO = 0;
struct ExampleReqEcho_t : public ExampleReqCmd_s {
    int8_t mLen;
    char mCmd[512];
    ExampleReqEcho_t() : ExampleReqCmd_s(EXAMPLE_REQ_ECHO) { }

    inline std::string cmd() {
        return mCmd;
    }
    inline void set_cmd(const std::string& value) {
        memcpy(mCmd, value.data(), value.size());
    }
};

const uint8_t CMD_EXAMPLE_RES = 51;
struct ExampleResCmd_s : public hnet::wCommand {
    ExampleResCmd_s(uint8_t para) : wCommand(CMD_EXAMPLE_RES, para) { }
};

const uint8_t EXAMPLE_RES_ECHO = 0;
struct ExampleResEcho_t : public ExampleResCmd_s {
    int8_t mLen;
    char mCmd[512];
    int32_t mRet;
    ExampleResEcho_t() : ExampleResCmd_s(EXAMPLE_RES_ECHO) { }

    inline std::string cmd() {
        return mCmd;
    }
    inline int32_t ret() {
        return mRet;
    }
    inline void set_cmd(const std::string& value) {
        memcpy(mCmd, value.data(), value.size());
    }
    inline void set_ret(int32_t ret) {
        mRet = ret;
    }
};

#pragma pack(1)
}   // namespace example

#endif
