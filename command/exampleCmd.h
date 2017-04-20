
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wCommand.h"

#ifndef _EXAMPLE_CMD_H_
#define _EXAMPLE_CMD_H_

const uint8_t CMD_EXAMPLE_REQ = 50;
struct ExampleReqCmd_s : public hnet::wCommand {
    ExampleReqCmd_s(uint8_t para) : wCommand(CMD_EXAMPLE_REQ, para) { }
};

const uint8_t EXAMPLE_REQ_ECHO = 0;
struct ExampleReqEcho_t : public ExampleReqCmd_s {
    int8_t mLen;
    char mCmd[512];
    ExampleReqEcho_t() : ExampleReqCmd_s(EXAMPLE_REQ_ECHO) { }
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
};

#endif
