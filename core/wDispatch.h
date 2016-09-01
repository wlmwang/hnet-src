
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

#define DEC_DISP(dispatch) wDispatch<function<int(char*, int)>, int> dispatch
#define DEC_FUNC(func) int func(char *pBuffer, int iLen)
#define REG_FUNC(ActIdx, vFunc) wDispatch<function<int(char*, int)>, int>::Func_t {ActIdx, std::bind(vFunc, this, std::placeholders::_1, std::placeholders::_2)}
#define REG_DISP(dispatch, classname, cmdid, paraid, func) dispatch.Register(classname, W_CMD(cmdid, paraid), REG_FUNC(W_CMD(cmdid, paraid), func));

namespace hnet {

// 每种回调Func_t（mFunc调用参数不同）需不同的wDispatch
template<typename T, typename IDX>
class wDispatch : private wNoncopyable {
public:
    struct Func_t {
        Func_t() {
            mActIdx = "";
        }
        Func_t(IDX ActIdx, T Func) {
            mActIdx = ActIdx;
            mFunc = Func;
        }
        
        IDX mActIdx;
        // T为function类型，例：function<void(void)>
        // mFunc用类似std::bind函数绑定，例：bind(&wTcpTask::Get, this, std::placeholders::_1)
        T mFunc;
    };
    
    wDispatch() : mProcNum(0) { }
    bool Register(string className, IDX ActIdx, struct Func_t vFunc);
    struct Func_t * GetFuncT(string className, IDX ActIdx);

protected:
    map<string, vector<struct Func_t> > mProc;	//注册回调方法
    int mProcNum;
};

template<typename T,typename IDX>
bool wDispatch<T,IDX>::Register(string className, IDX ActIdx, struct Func_t vFunc) {
    vector<struct Func_t> vf;
    typename map<string, vector<struct Func_t> >::iterator mp = mProc.find(className);
    if (mp != mProc.end()) {
        vf = mp->second;
        mProc.erase(mp);

        typename vector<struct Func_t>::iterator itvf = vf.begin();
        for (; itvf != vf.end() ; itvf++) {
            if (itvf->mActIdx == ActIdx) {
                itvf = vf.erase(itvf);
                itvf--;
            }
        }
    }
    vf.push_back(vFunc);
    //pair<map<string, vector<struct Func_t> >::iterator, bool> itRet;
    //itRet = 
    mProc.insert(pair<string, vector<struct Func_t> >(className, vf));
    mProcNum++;
    return true;
    //return itRet.second;
}

template<typename T,typename IDX>
struct wDispatch<T,IDX>::Func_t* wDispatch<T,IDX>::GetFuncT(string className, IDX ActIdx) {
    typename map<string, vector<struct Func_t> >::iterator mp = mProc.find(className);
    if (mp != mProc.end()) {
        typename vector<struct Func_t>::iterator itvf = mp->second.begin();
        for (; itvf != mp->second.end() ; itvf++) {
            if (itvf->mActIdx == ActIdx) {
                return &*itvf;
            }
        }
    }
    return 0;
}

}   // namespace hnet

#endif