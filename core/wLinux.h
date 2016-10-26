
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_LINUX_H_
#define _W_LINUX_H_

// off_t 突破单文件4G大小限制，系统库sys/types.h中使用
#define _FILE_OFFSET_BITS  64

#include <unistd.h>
#include <sched.h>
#include <sys/time.h>

#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/resource.h>

#include <stdio.h>

#endif
