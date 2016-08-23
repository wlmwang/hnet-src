
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

/**
 * code is far away from bug with the animal protecting
 *  ┏┓　　　┏┓
 *┏┛┻━━━┛┻┓
 *┃　　　　　　　┃ 　
 *┃　　　━　　　┃
 *┃　┳┛　┗┳　┃
 *┃　　　　　　　┃
 *┃　　　┻　　　┃
 *┃　　　　　　　┃
 *┗━┓　　　┏━┛
 *　　┃　　　┃神兽保佑
 *　　┃　　　┃代码无BUG！
 *　　┃　　　┗━━━┓
 *　　┃　　　　　　　┣┓
 *　　┃　　　　　　　┏┛
 *　　┗┓┓┏━┳┓┏┛
 *　　　┃┫┫　┃┫┫
 *　　　┗┻┛　┗┻┛ 
 *
 */

#ifndef _W_CORE_H_
#define _W_CORE_H_

#include "wLinux.h"
#include "wDef.h"

#define ALIGNMENT	sizeof(unsigned long)
#define ALIGN(d, a)		(((d) + (a - 1)) & ~(a - 1))
#define ALIGN_PTR(p, a)	(u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

#define SAFE_NEW(type, ptr) \
    do \
    { \
        try \
        { \
            ptr = NULL; \
            ptr = new type; \
        } \
        catch (...) \
        { \
            ptr = NULL; \
        } \
    } while (0)

#define SAFE_NEW_VEC(n, type, ptr) \
    do \
    { \
        try \
        { \
            ptr = NULL; \
            ptr = new type[n]; \
        } \
        catch (...) \
        { \
            ptr = NULL; \
        } \
    } while(0)

#define SAFE_DELETE(ptr) \
    do \
    { \
    	if(ptr) { \
    		delete ptr; \
    		ptr = NULL; \
    	} \
    } while(0)

#define SAFE_DELETE_VEC(pointer) \
    do \
    { \
        delete[] pointer; \
        pointer = 0; \
    } while(0)
	
#define SAFE_FREE(ptr) \
    do \
    { \
    	if(ptr) { \
    		free(ptr); \
    		ptr = NULL; \
    	} \
    } while(0)

//命名空间
using namespace std;
extern char **environ;

#endif
