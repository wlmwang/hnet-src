
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

#define DEC_DISP(dispatch) hnet::wDispatch<std::function<int(char*, uint32_t)>, uint16_t> dispatch
#define DEC_FUNC(func) int func(char *buf, uint32_t len)
#define REG_FUNC(seq, func) hnet::wDispatch<std::function<int(char*, uint32_t)>, uint16_t>::Func_t {seq, std::bind(func, this, std::placeholders::_1, std::placeholders::_2)}
#define REG_DISP(dispatch, classname, cmd, para, func) dispatch.Register(classname, hnet::CmdId(cmd, para), REG_FUNC(hnet::CmdId(cmd, para), func))

namespace hnet {

class wTask;
typedef int (wTask::*TaskDisp)(char*, uint32_t);

// 简单路由
template<typename T, typename IDX = uint16_t>
class wDispatch : private wNoncopyable {
public:
    // 函数结构
    struct Func_t;

    wDispatch() : mProcNum(0) { }
    bool Register(std::string className, IDX seq, struct Func_t func);
    struct Func_t* GetFuncT(string className, IDX seq);

protected:
    std::map<std::string, std::vector<struct Func_t> > mProc;	// 注册回调方法
    int mProcNum;
};

template<typename T, typename IDX>
struct wDispatch<T, IDX>::Func_t {
    Func_t() : mSeq("") { }
    Func_t(IDX seq, T Func) : mSeq(seq), mFunc(Func) { }

    // 路由id，一般每个消息头都有一个表示消息类型
    IDX mSeq;
    // T为function类型，例：function<int(char*, int)>
    // mFunc用类似std::bind函数绑定，例：bind(&wTcpTask::Get, this, std::placeholders::_1)
    T mFunc;
};

template<typename T, typename IDX>
bool wDispatch<T, IDX>::Register(string className, IDX seq, struct Func_t func) {
    std::vector<struct Func_t> vf;
    typename std::map<string, vector<struct Func_t> >::iterator mp = mProc.find(className);
    if (mp != mProc.end()) {
        vf = mp->second;
        mProc.erase(mp);

        typename vector<struct Func_t>::iterator itvf = vf.begin();
        for (; itvf != vf.end() ; itvf++) {
            if (itvf->mSeq == seq) {
                itvf = vf.erase(itvf);
                itvf--;
            }
        }
    }
    vf.push_back(func);
    //pair<map<string, vector<struct Func_t> >::iterator, bool> itRet;
    //itRet = 
    mProc.insert(std::pair<std::string, std::vector<struct Func_t> >(className, vf));
    mProcNum++;
    return true;
    //return itRet.second;
}

template<typename T, typename IDX>
struct wDispatch<T, IDX>::Func_t* wDispatch<T,IDX>::GetFuncT(string className, IDX seq) {
    typename std::map<string, std::vector<struct Func_t> >::iterator mp = mProc.find(className);
    if (mp != mProc.end()) {
        typename std::vector<struct Func_t>::iterator itvf = mp->second.begin();
        for (; itvf != mp->second.end() ; itvf++) {
            if (itvf->mSeq == seq) {
                return &*itvf;
            }
        }
    }
    return 0;
}

}   // namespace hnet

#endif