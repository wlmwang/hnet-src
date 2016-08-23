
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_PROC_TITLE_H_
#define _W_PROC_TITLE_H_

#include "wCore.h"
#include "wMisc.h"
#include "wNoncopyable.h"

class wProcTitle : private wNoncopyable
{
	public:
		wProcTitle(int argc, const char *argv[]);
		~wProcTitle();

		//务必在设置进程标题之前调用
		void SaveArgv();

		/**
		 *  移动**environ到堆上，为进程标题做准备。计算**environ指针结尾地址。
		 *  tips：*argv[]与**environ两个变量所占的内存是连续的，并且是**environ紧跟在*argv[]后面
		 */
		int InitSetproctitle();

		//设置进程标题
		void Setproctitle(const char *title, const char *pretitle = NULL);

	public:
		int mArgc {0};
		char **mOsArgv {NULL};	//原生参数
		char **mOsEnv {NULL};	//原生环境变量
		char **mArgv {NULL};	//堆上参数（正常取该值）
		char *mOsArgvLast {NULL};
};

#endif