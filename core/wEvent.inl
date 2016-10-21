
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

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
