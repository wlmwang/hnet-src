
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_COMMAND_H_
#define _W_COMMAND_H_

#include "wCore.h"

#define W_CMD(cmd, para) ((para)<<8 | (cmd))

#pragma pack(1)

const BYTE CMD_NULL = 0;
const BYTE PARA_NULL = 0;

enum
{
	SERVER,
	CLIENT,
};

struct _Null_t
{
	_Null_t(const BYTE cmd, const BYTE para) : mCmd(cmd), mPara(para) {}
	WORD GetId() const { return mId; }
	BYTE GetCmd() const { return mCmd; }
	BYTE GetPara() const { return mPara; }
	
	union
	{
		WORD mId;
		struct {BYTE mCmd; BYTE mPara;};
	};
};

struct wCommand : public _Null_t
{
	wCommand(const BYTE cmd = CMD_NULL, const BYTE para = PARA_NULL) : _Null_t(cmd, para) {}
};

#pragma pack()

#endif
