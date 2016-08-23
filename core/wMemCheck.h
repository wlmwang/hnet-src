
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MEM_CHECK_H_
#define _W_MEM_CHECK_H_

#include <malloc.h>
#include <new>
#include <map>

#include "wCore.h"
#include "wLog.h"

#if _MEM_CHECK
#define MALLOC(x)	malloc_debug(x, __FILE__, __LINE__)

#define FREE(x)		free_debug(x, __FILE__, __LINE__)
#define FREE_IF(x)	do { if((x) != 0) free_debug((void *)(x), __FILE__, __LINE__); } while(0)
#define FREE_CLEAR(x)	do { if((x) != 0) { free_debug((void *)(x), __FILE__, __LINE__); (x)=0; } } while(0)

#define REALLOC(p, sz)	({ void *a=realloc_debug(p,sz, __FILE__, __LINE__); if(a) p = (typeof(p))a; a; })
#define CALLOC(x, y)	calloc_debug(x, y, __FILE__, __LINE__)
#define STRDUP(x)	strdup_debug(x, __FILE__, __LINE__)

void *malloc_debug(size_t, const char *, int);
void free_debug(void *, const char *, int);
void *realloc_debug(void *, size_t, const char *, int);
void *calloc_debug(size_t, size_t, const char *, int);
char *strdup_debug(const char *, const char *, int);

#else
#define MALLOC(x)	malloc(x)

#define FREE(x)		free(x)
#define FREE_IF(x)	do { if((x) != 0) free((void *)(x)); } while(0)
#define FREE_CLEAR(x)	do { if((x) != 0) { free((void *)(x)); (x)=0; } } while(0)

#define REALLOC(p, sz)	({ void *a=realloc(p,sz); if(a) p = (typeof(p))a; a; })
#define CALLOC(x, y)	calloc(x, y)
#define STRDUP(x)	strdup(x)
#endif

#if __cplusplus

#if _MEM_CHECK
void dump_non_delete();
void ReportMallinfo();
unsigned long CountVirtualSize();
unsigned long count_alloc_size();
#else
static inline void dump_non_delete() {}
static inline void ReportMallinfo() {}
#endif

#endif
