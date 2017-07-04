
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
    wProcTitle();
    
    ~wProcTitle();

    // 务必在设置进程标题之前调用
    // 移动**argv到堆上，移动**environ到堆上
    // tips：*argv[]与**environ两个指针数组指向的值所占的内存是连续的，且**environ紧跟在*argv[]后面
    int SaveArgv(int argc, char* argv[]);

    // 设置进程标题
    // Linux进程名称是通过命令行参数argv[0]来表示的
    int Setproctitle(const char *title, const char *pretitle = NULL, bool attach = true);

    char** Argv() { return mArgv;}
    char** Environ() { return mEnv;}

protected:
    int mOsEnvc;
    int mOsArgc;
    char** mOsEnv;
    char** mOsArgv;

    char** mArgv;
    char** mEnv;
};

}	// namespace hnet

#endif
