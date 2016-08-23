
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_LINUX_H_
#define _W_LINUX_H_

#define _FILE_OFFSET_BITS  64	//off_t 突破单文件4G大小限制，系统库sys/types.h中使用

#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <sys/wait.h>

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <pwd.h>
#include <grp.h>

//#include <dirent.h>
//#include <sys/mman.h>

#include <stdio.h>
#include <iostream>
#include <string>

//单字节无符号整数
typedef uint8_t BYTE;

//双字节无符号整数
typedef uint16_t WORD;

//双字节符号整数
typedef int16_t SWORD;

//四字节无符号整数
typedef uint32_t DWORD;

//四字节符号整数
typedef int32_t SDWORD;

//八|十六字节无符号整数
typedef uint64_t QWORD;

//八|十六字节符号整数
typedef int64_t SQWORD;

//八字节无符号整数
typedef unsigned long int LWORD;

//八字节符号整数
typedef signed long int SLWORD;

//十六节无符号整数
typedef unsigned long long int LLWORD;

//十六字节符号整数
typedef signed long long int SLLWORD;

#endif