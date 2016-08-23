
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_NONCOPYABLE_H_
#define _W_NONCOPYABLE_H_

/**
 *  禁止复制、赋值
 */
class wNoncopyable
{
	protected:
		wNoncopyable() {};
		~wNoncopyable() {};
		
	private:
		wNoncopyable(const wNoncopyable&);
		const wNoncopyable & operator= (const wNoncopyable &);
};

#endif

