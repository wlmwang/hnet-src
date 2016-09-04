
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_PROC_TITLE_H_
#define _W_PROC_TITLE_H_

#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

class wProcTitle : private wNoncopyable {
public:
    wProcTitle(int argc, const char *argv[]);
    ~wProcTitle();

    // 务必在设置进程标题之前调用
    void SaveArgv();

    // 移动**environ到堆上，为进程标题做准备。计算**environ指针结尾地址
    // tips：*argv[]与**environ两个变量所占的内存是连续的，并且是**environ紧跟在*argv[]后面
    int InitSetproctitle();

    // 设置进程标题
    void Setproctitle(const char *title, const char *preTitle = NULL);

public:
    int mArgc;
    char *mOsArgvLast;
    char **mOsArgv;	//原生参数
    char **mOsEnv;	//原生环境变量
    char **mArgv;	//堆上参数（正常取该值）
};

}	// namespace hnet

#endif
