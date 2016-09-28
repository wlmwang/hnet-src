
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_LOGIN_CMD_H_
#define _W_LOGIN_CMD_H_

#include "wCore.h"
#include "wCommand.h"

#pragma pack(1)

//++++++++++++请求数据结构
const uint8_t CMD_LOGIN_REQ = 2;
struct LoginReqCmd_s : public wCommand {
	LoginReqCmd_s(uint8_t para) : wCommand(CMD_LOGIN_REQ, para) { }
};

const uint8_t LOGIN_REQ_TOKEN = 0;
struct LoginReqToken_t : public LoginReqCmd_s {
	LoginReqToken_t() : LoginReqCmd_s(LOGIN_REQ_TOKEN) {
		memset(mToken, 0, 32);
	}
	uint8_t mConnType;
	char mToken[32];
};

//++++++++++++返回
const uint8_t CMD_LOGIN_RES = 2;
struct LoginResCmd_s : public wCommand {
	LoginResCmd_s(uint8_t para) : wCommand(CMD_LOGIN_RES, para) { }
};

const uint8_t LOGIN_RES_STATUS = 1;
struct LoginResStatus_t : public LoginResCmd_s {
	LoginResStatus_t() : LoginResCmd_s(LOGIN_RES_STATUS) {
		mStatus = 0;
	}
	char mStatus;
};

#pragma pack()

#endif