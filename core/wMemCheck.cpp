
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMemCheck.h"

#if _MEM_CHECK

void ReportMallinfo()
{
	struct mallinfo stMi = mallinfo();
	
	LOG_DEBUG(ELOG_KEY, "[mallinfo] arena: %d\n", stMi.arena);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] ordblks: %d\n", stMi.ordblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] smblks: %d\n", stMi.smblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] hblks: %d\n", stMi.hblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] usmblks: %d\n", stMi.usmblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] fsmblks: %d\n", stMi.fsmblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] uordblks: %d\n", stMi.uordblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] fordblks: %d\n", stMi.fordblks);
	LOG_DEBUG(ELOG_KEY, "[mallinfo] keepcost: %d\n", stMi.keepcost);
}

unsigned long CountVirtualSize(void)
{
	FILE *pFile = fopen("/proc/self/maps", "r");
	char pBuf[256];
	pBuf[sizeof(pBuf) - 1] = '\0';
	
	unsigned long iTotal = 0;
	while(fgets(pBuf, sizeof(pBuf) - 1, pFile) != NULL)
	{
		unsigned long iStart, iEnd;
		char r,w,x,p;
		
		if(sscanf(pBuf, "%lx-%lx %c%c%c%c", &iStart, &iEnd, &r, &w, &x, &p) != 6) 
			continue;
		if(r == '-' && w == '-' && x == '-')
			continue;
		if(r != 'r') 
			continue;
		if(p == 's') 
			continue;
		
		iTotal += iEnd - iStart;
	}
	
	fclose(pFile);
	
	return iTotal;
}

void *operator new(size_t iSize)
{
	const char *ret = (const char *)__builtin_return_address(0);
	if(iSize == 0)
	{
		return 0;
	}
	
	void *ptr = malloc(iSize);
	if(ptr == NULL)
	{
		LOG_ERROR(ELOG_KEY, "[system] ret@%p: operator new(%ld) failed", ret, (long)iSize);
	}
	else
	{
		//add_ptr(ptr, iSize, ret, 0);
	}
	return ptr;
}

void *operator new[](size_t iSize)
{
	return operator new(iSize);
}

void operator delete(void *ptr)
{
	if(ptr)
	{
		//del_ptr(ptr);
	}
	return free(ptr);
}

void operator delete[](void *ptr)
{
	return operator delete(ptr);
}

#endif

