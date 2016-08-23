
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMemPool.h"

wMemPool::~wMemPool() 
{
	Destroy();
}

char *wMemPool::Create(size_t size)
{
    mStart = (char *) memalign(POOL_ALIGNMENT, size);
    if(mStart == NULL)
	{
        return NULL;
    }
	
	mSize = size;
    mLast = mStart;
    mEnd  = mStart + size;
	
    return mStart;
}

char *wMemPool::Alloc(size_t size)
{
	char *m = NULL;
	if(m + size <= mEnd)
	{
		m = (char *) ALIGN_PTR(mLast, ALIGNMENT);
		mLast = m + size;
	}
	else
	{
		struct extra_t *l = (struct extra_t *) memalign(POOL_ALIGNMENT, sizeof(struct extra_t));
		m = (char *) memalign(POOL_ALIGNMENT, size);
		
		l->mAddr = m;
		l->mNext = mExtra;
		mExtra = l;
	}
	return m;
}


void wMemPool::Destroy()
{
    for (struct extra_t* l = mExtra; l; l = l->mNext) 
	{
        if (l->mAddr) 
		{
            free(l->mAddr);
        }
    }
	
	if(mStart)
	{
		free(mStart);
	}
}

void wMemPool::Reset()
{
	mLast = mStart;
    
	for (struct extra_t* l = mExtra; l; l = l->mNext) 
	{
        if (l->mAddr) 
		{
            free(l->mAddr);
        }
    }
}
