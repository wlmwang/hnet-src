
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_EVENT_H_
#define _W_EVENT_H_

#include <map>
#include <list>
#include <functional>
#include "wCore.h"
#include "wNoncopyable.h"
#include "wCommand.h"

namespace hnet {

// EVENT	事件类型，一般为每个消息头的消息类型字段  uint16_t
// FUNC		std::function类型，如std::function<void(void*)>类型。
//			具体使用中一般用std::bind绑定具体函数，如std::bind(&ClassName::Method, target /*this*/, std::placeholders::_1)
// ARGV		参数类型指针
template<typename EVENT, typename FUNC, typename ARGV = void*>
class wEvent : private wNoncopyable {
public:
	wEvent() { }
    bool On(EVENT ev, FUNC listener, bool multicast = true);
    //bool Once(EVENT ev, FUNC listener, bool multicast = true);
    bool RemoveListener(EVENT *ev = NULL, FUNC *listener = NULL);

    bool Emit(EVENT ev, ARGV argv = NULL);
    bool operator()(EVENT ev, ARGV argv = NULL);
protected:
    std::map<EVENT, std::list<FUNC> > mEventListener;

    typedef typename std::map<EVENT, std::list<FUNC> > MapType_t;
    typedef typename std::map<EVENT, std::list<FUNC> >::iterator MapTypeIt_t;
    typedef typename std::list<FUNC> ListType_t;
    typedef typename std::list<FUNC>::iterator ListTypeIt_t;
    typedef typename std::pair<EVENT, std::list<FUNC> > PairType_t;
};

template<typename EVENT, typename FUNC, typename ARGV>
bool wEvent<EVENT, FUNC, ARGV>::On(EVENT ev, FUNC listener, bool multicast) {
    ListType_t listfunc;
    MapTypeIt_t itmap = mEventListener.find(ev);
    if (itmap != mEventListener.end()) {
        listfunc = itmap->second;
        mEventListener.erase(itmap);
        // 非多播
        if (!multicast) {
            listfunc.clear();
        }
    }
    listfunc.push_back(listener);
    mEventListener.insert(PairType_t(ev, listfunc));
    return true;
}

template<typename EVENT, typename FUNC, typename ARGV>
bool wEvent<EVENT, FUNC, ARGV>::Emit(EVENT ev, ARGV argv) {
    return (*this)(ev, argv);
}

template<typename EVENT, typename FUNC, typename ARGV>
bool wEvent<EVENT, FUNC, ARGV>::operator()(EVENT ev, ARGV argv) {
    MapTypeIt_t itmap = mEventListener.find(ev);
    if (itmap != mEventListener.end()) {
        ListTypeIt_t itlist = itmap->second.begin();
        for (; itlist != itmap->second.end(); itlist++) {
            (*itlist)(argv);
        }
        return true;
    }
    return false;
}

template<typename EVENT, typename FUNC, typename ARGV>
bool wEvent<EVENT, FUNC, ARGV>::RemoveListener(EVENT *ev, FUNC *listener) {
    if (ev == NULL) {
        mEventListener.clear();
        return true;
    }
    MapTypeIt_t itmap = mEventListener.find(*ev);
    if (itmap != mEventListener.end()) {
        ListType_t listfunc = itmap->second;
        mEventListener.erase(itmap);
        if (listener == NULL) {
            return true;
        }
        ListTypeIt_t itlist = listfunc.find(*listener);
        if (itlist != listfunc.end()) {
            listfunc.erase(itlist);
            if (listfunc.size() > 0) {
                mEventListener.insert(PairType_t(*ev, listfunc));
            }
            return true;
        }
    }
    return false;
}

}   // namespace hnet

#endif
