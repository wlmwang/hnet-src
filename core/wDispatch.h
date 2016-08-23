
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_DISPATCH_H_
#define _W_DISPATCH_H_

#include <map>
#include <vector>
#include <functional>

#include "wCore.h"
#include "wNoncopyable.h"

/**
 * Dispatch
 */
#define DEC_DISP(dispatch) wDispatch<function<int(char*, int)>, int> dispatch
#define DEC_FUNC(func) int func(char *pBuffer, int iLen)
#define REG_FUNC(ActIdx, vFunc) wDispatch<function<int(char*, int)>, int>::Func_t {ActIdx, std::bind(vFunc, this, std::placeholders::_1, std::placeholders::_2)}
#define REG_DISP(dispatch, classname, cmdid, paraid, func) dispatch.Register(classname, W_CMD(cmdid, paraid), REG_FUNC(W_CMD(cmdid, paraid), func));

/**
 * 每种回调Func_t（mFunc调用参数不同）需不同的wDispatch
 */
template<typename T,typename IDX>
class wDispatch : private wNoncopyable
{
	public:
		struct Func_t
		{
			IDX mActIdx;
			/**
			 * T为function类型，例：function<void(void)>
			 * mFunc用类似std::bind函数绑定，例：bind(&wTcpTask::Get, this, std::placeholders::_1)
			 */
			T mFunc;
			Func_t()
			{
				mActIdx = "";
			}
			Func_t(IDX ActIdx, T Func)
			{
				mActIdx = ActIdx;
				mFunc = Func;
			}
		};

		bool Register(string className, IDX ActIdx, struct Func_t vFunc);
		struct Func_t * GetFuncT(string className, IDX ActIdx);

	protected:
		map<string, vector<struct Func_t> > mProc;	//注册回调方法
		int mProcNum {0};
};

#include "wDispatch.inl"

#endif