
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

#include "wEvent.inl"

}   // namespace hnet

#endif
