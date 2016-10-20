
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_DISPATCH_H_
#define _W_DISPATCH_H_

#include <map>
#include <list>
#include <functional>
#include "wCore.h"
#include "wNoncopyable.h"
#include "wCommand.h"

namespace hnet {

// FUNC		std::function类型，如std::function<void(void*)>类型。具体使用中一般用std::bind绑定具体函数，如std::bind(&ClassName::Method, target /*this*/, std::placeholders::_1)
// ARGV		参数类型
// IDENTIFY	路由关键字，一般为每个消息头的消息类型字段
template<typename FUNC, typename ARGV = void*, typename IDENTIFY = uint16_t>
class wDispatch : private wNoncopyable {
public:
    wDispatch() { }
    bool AddHandler(IDENTIFY idenfify, FUNC func, bool multicast = true);
    bool operator()(IDENTIFY idenfify, ARGV argv = NULL);
protected:
    std::map<IDENTIFY, std::list<FUNC> > mHandler;

    typedef typename std::map<IDENTIFY, std::list<FUNC> > MapType_t;
    typedef typename std::map<IDENTIFY, std::list<FUNC> >::iterator MapTypeIt_t;
    typedef typename std::list<FUNC> ListType_t;
    typedef typename std::list<FUNC>::iterator ListTypeIt_t;
};

template<typename FUNC, typename ARGV, typename IDENTIFY>
bool wDispatch<FUNC, ARGV, IDENTIFY>::AddHandler(IDENTIFY idenfify, FUNC func, bool multicast) {
	ListType_t listfunc;
	MapTypeIt_t itmap = mHandler.find(idenfify);
	if (itmap != mHandler.end()) {
		listfunc = itmap->second;
		mHandler.erase(itmap);
		// 非多播
		if (!multicast) {
			listfunc.clear();
		}
	}
	listfunc.push_back(func);
	mHandler.insert(std::pair<IDENTIFY, std::list<FUNC> >(idenfify, listfunc));
    return true;
}

template<typename FUNC, typename ARGV, typename IDENTIFY>
bool wDispatch<FUNC, ARGV, IDENTIFY>::operator()(IDENTIFY idenfify, ARGV argv) {
	MapTypeIt_t itmap = mHandler.find(idenfify);
	if (itmap != mHandler.end()) {
		ListTypeIt_t itlist = itmap->second.begin();
		for (; itlist != itmap->second.end(); itlist++) {
			(*itlist)(argv);
		}
		return true;
	}
	return false;
}

}   // namespace hnet

#endif
