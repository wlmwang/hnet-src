
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_NONCOPYABLE_H_
#define _W_NONCOPYABLE_H_

namespace hnet {

// 禁止拷贝、赋值
class wNoncopyable {
	protected:
		wNoncopyable() { }
		~wNoncopyable() { }
		
	private:
		wNoncopyable(const wNoncopyable&);
		const wNoncopyable & operator= (const wNoncopyable &);
};
}	// namespace hnet

#endif

