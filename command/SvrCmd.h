
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _SVR_CMD_H_
#define _SVR_CMD_H_

#include "wCore.h"
#include "wCommand.h"
#include "Common.h"
#include "Svr.h"

#pragma pack(1)

/** 服务端消息通信 router、agent */
//++++++++++++请求数据结构
const BYTE CMD_SVR_REQ = 60;
struct SvrReqCmd_s : public wCommand
{
	SvrReqCmd_s(BYTE para)
	{
		mCmd = CMD_SVR_REQ;
		mPara = para;
	}
};

//agent主动初始化请求
const BYTE SVR_REQ_INIT = 0;
struct SvrReqInit_t : public SvrReqCmd_s 
{
	SvrReqInit_t() : SvrReqCmd_s(SVR_REQ_INIT) {}
};

//router重新读取配置（agent重读配置由初始化请求替代）
const BYTE SVR_REQ_RELOAD = 1;
struct SvrReqReload_t : public SvrReqCmd_s 
{
	SvrReqReload_t() : SvrReqCmd_s(SVR_REQ_RELOAD) {}
};

//router主动下发同步svr
const BYTE SVR_REQ_SYNC = 2;
struct SvrReqSync_t : public SvrReqCmd_s 
{
	SvrReqSync_t() : SvrReqCmd_s(SVR_REQ_SYNC) {}
};

//获取全部svr
const BYTE SVR_REQ_ALL = 3;
struct SvrReqAll_t : public SvrReqCmd_s 
{
	SvrReqAll_t() : SvrReqCmd_s(SVR_REQ_ALL) {}
};

//根据gid、xid获取svr
const BYTE SVR_REQ_GXID = 4;
struct SvrReqGXid_t : public SvrReqCmd_s 
{
	SvrReqGXid_t() : SvrReqCmd_s(SVR_REQ_GXID), mGid(0),mXid(0) {}
	
	SDWORD mGid;
	SDWORD mXid;
};

/** 上报数据结构 */
const BYTE SVR_REQ_REPORT = 5;
struct SvrReqReport_t : public SvrReqCmd_s
{
	SvrReqReport_t() : SvrReqCmd_s(SVR_REQ_REPORT)
	{
		memset(&mCaller, 0, sizeof(mCaller));
	}
	struct SvrCaller_t mCaller;
};

//++++++++++++返回数据结构
const BYTE CMD_SVR_RES = 61;
struct SvrResCmd_s : public wCommand
{
	SvrResCmd_s(BYTE para)
	{
		mCmd = CMD_SVR_RES;
		mPara = para;
		mCode = 0;
	}
	short mCode;
};

//初始化返回
const BYTE SVR_RES_INIT = 0;
struct SvrResInit_t : public SvrResCmd_s 
{
	SvrResInit_t() : SvrResCmd_s(SVR_RES_INIT) 
	{
		mNum = 0;
		memset(mSvr, 0, sizeof(mSvr));
	}
	int mNum;
	SvrNet_t mSvr[MAX_SVR_NUM];
};

//重载返回
const BYTE SVR_RES_RELOAD = 1;
struct SvrResReload_t : public SvrResCmd_s 
{
	SvrResReload_t() : SvrResCmd_s(SVR_RES_RELOAD) 
	{
		mNum = 0;
		memset(mSvr, 0, sizeof(mSvr));
	}
	int mNum;
	SvrNet_t mSvr[MAX_SVR_NUM];
};

//同步返回
const BYTE SVR_RES_SYNC = 2;
struct SvrResSync_t : public SvrResCmd_s 
{
	SvrResSync_t() : SvrResCmd_s(SVR_RES_SYNC) 
	{
		mNum = 0;
		memset(mSvr, 0, sizeof(mSvr));
	}
	int mNum;
	SvrNet_t mSvr[MAX_SVR_NUM];
};

//获取svr请求返回数据，包括all|id|name|gid/xid
const BYTE SVR_RES_DATA = 3;
struct SvrResData_t : public SvrResCmd_s 
{
	SvrResData_t() : SvrResCmd_s(SVR_RES_DATA) 
	{
		mReqId = 0;
		mNum = 0;
		memset(mSvr, 0, sizeof(mSvr));
	}
	short mReqId;	//包括all|id|name|gid/xid的wCommand::mId  区分请求cmd
	int mNum;
	SvrNet_t mSvr[MAX_SVR_NUM];
};

//获取单个svr
const BYTE SVR_ONE_RES = 4;
struct SvrOneRes_t : public SvrResCmd_s 
{
	SvrOneRes_t() : SvrResCmd_s(SVR_ONE_RES) 
	{
		mNum = 0;
		memset(&mSvr, 0, sizeof(mSvr));
	}
	int mNum;
	SvrNet_t mSvr;
};

//上报返回
const BYTE SVR_RES_REPORT = 5;
struct SvrResReport_t : public SvrResCmd_s 
{
	SvrResReport_t() : SvrResCmd_s(SVR_RES_REPORT) 
	{
		//
	}
};

#pragma pack()

#endif