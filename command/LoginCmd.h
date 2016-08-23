
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _LOGIN_CMD_H_
#define _LOGIN_CMD_H_

#include "wCore.h"
#include "Common.h"

#pragma pack(1)

//++++++++++++请求数据结构
const BYTE CMD_LOGIN_REQ = 50;
struct LoginReqCmd_s : public wCommand
{
	LoginReqCmd_s(BYTE para)
	{
		mCmd = CMD_LOGIN_REQ;
		mPara = para;
	}
};

const BYTE LOGIN_REQ_TOKEN = 0;
struct LoginReqToken_t : public LoginReqCmd_s 
{
	LoginReqToken_t() : LoginReqCmd_s(LOGIN_REQ_TOKEN)
	{
		memset(mToken, 0, 32);
	}
	BYTE mConnType;
	char mToken[32];
};


//++++++++++++返回
const BYTE CMD_LOGIN_RES = 51;
struct LoginResCmd_s : public wCommand
{
	LoginResCmd_s(BYTE para)
	{
		mCmd = CMD_LOGIN_RES;
		mPara = para;
	}
};

const BYTE LOGIN_RES_STATUS = 0;
struct LoginResStatus_t : public LoginResCmd_s 
{
	LoginResStatus_t() : LoginResCmd_s(LOGIN_RES_STATUS)
	{
		mStatus = 0;
	}
	
	char mStatus;
};

#pragma pack()

#endif