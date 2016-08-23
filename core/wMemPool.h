
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MEM_POOL_H_
#define _W_MEM_POOL_H_

#include <malloc.h>
#include <new>

#include "wCore.h"
#include "wNoncopyable.h"

#define POOL_ALIGNMENT 4095

class wMemPool : private wNoncopyable
{
	struct extra_t
	{
		struct extra_t	*mNext {NULL};
		char	*mAddr {NULL};
	};

	public:
		wMemPool() {}
		virtual ~wMemPool();

		char *Create(size_t size);
		char *Alloc(size_t size);
		void Destroy();
		void Reset();
		
	protected:
		char	*mStart {NULL}; //起始地址
		char	*mLast {NULL};	//已分配到的地址
		char	*mEnd {NULL};	//结束地址
		int		mSize {0};
		struct extra_t *mExtra {NULL};
};

#endif
