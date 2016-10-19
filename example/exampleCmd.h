
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wCommand.h"

#ifndef _W_EXAMPLE_H_
#define _W_EXAMPLE_H_

const uint8_t CMD_EXAMPLE_REQ = 100;
struct ExampleReqCmd_s : public hnet::wCommand {
    ExampleReqCmd_s(uint8_t para) : wCommand(CMD_EXAMPLE_REQ, para) { }
};

const uint8_t EXAMPLE_REQ_ECHO = 0;
struct ExampleReqEcho_t : public ExampleReqCmd_s {
    int8_t mLen;
    char mCmd[512];
    ExampleReqEcho_t() : ExampleReqCmd_s(EXAMPLE_REQ_ECHO) { }
};

#endif
