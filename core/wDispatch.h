
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
#include "wCommand.h"

typedef int (*DispatchFunc)(char*, uint32_t);
#define DEC_DISP(dispatch) hnet::wDispatch<function<int(char*, uint32_t)>, uint16_t> dispatch
#define DEC_FUNC(func) int func(char *buf, uint32_t len)
#define REG_FUNC(ActIdx, vFunc) hnet::wDispatch<function<int(char*, uint32_t)>, uint16_t>::Func_t {ActIdx, std::bind(vFunc, this, std::placeholders::_1, std::placeholders::_2)}
#define REG_DISP(dispatch, classname, cmd, para, func) dispatch.Register(classname, hnet::CmdId(cmd, para), REG_FUNC(hnet::CmdId(cmd, para), func));

namespace hnet {

// 简单路由
template<typename T, typename IDX = uint16_t>
class wDispatch : private wNoncopyable {
public:
    struct Func_t;

    wDispatch() : mProcNum(0) { }
    bool Register(string className, IDX ActIdx, struct Func_t vFunc);
    struct Func_t* GetFuncT(string className, IDX ActIdx);
protected:
    map<string, vector<struct Func_t> > mProc;	// 注册回调方法
    int mProcNum;
};

template<typename T, typename IDX>
struct wDispatch<T, IDX>::Func_t {
    // 路由id，一般每个消息头都有一个表示消息类型
    IDX mActIdx;
    // T为function类型，例：function<int(char*, int)>
    // mFunc用类似std::bind函数绑定，例：bind(&wTcpTask::Get, this, std::placeholders::_1)
    T mFunc;

    Func_t() : mActIdx("") { }
    Func_t(IDX ActIdx, T Func) : mActIdx(ActIdx), mFunc(Func) { }
};

template<typename T, typename IDX>
bool wDispatch<T, IDX>::Register(string className, IDX ActIdx, struct Func_t vFunc) {
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

template<typename T, typename IDX>
struct wDispatch<T, IDX>::Func_t* wDispatch<T,IDX>::GetFuncT(string className, IDX ActIdx) {
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