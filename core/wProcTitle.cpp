
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wProcTitle.h"

namespace hnet {

extern char **environ;

wProcTitle::wProcTitle(int argc, const char *argv[]) {
    mArgc = argc;
    mOsArgv = (char **) argv;
    mOsArgvLast = (char *)argv[0];
    mOsEnv = environ;

    SaveArgv();
}

wProcTitle::~wProcTitle() {
    for (int i = 0; i < mArgc; ++i) {
        misc::SafeDeleteVec(mArgv[i]);
    }
    misc::SafeDeleteVec(mArgv);
}

void wProcTitle::SaveArgv() {
    // 初始化argv内存空间
    size_t size = 0;
    mArgv = new char*[mArgc];
    for(int i = 0; i < mArgc; ++i) {
        size = strlen(mOsArgv[i]) + 1;  //包含\0结尾
        mArgv[i] = new char[size];
        memcpy(mArgv[i], mOsArgv[i], size);
    }
}

int wProcTitle::InitSetproctitle() {
    // environ字符串总长度
    size_t size = 0;
    for (int i = 0; environ[i]; i++) {
        size += strlen(environ[i]) + 1;     
    }

    u_char* p = new u_char[size];
    if (p == NULL) {
        return -1;
    }

    //argv字符总长度
    for (int i = 0; mOsArgv[i]; i++) {
        if (mOsArgvLast == mOsArgv[i]) {
            mOsArgvLast = mOsArgv[i] + strlen(mOsArgv[i]) + 1;
        }
    }

    //移动**environ到堆上
    for (int i = 0; environ[i]; i++) {
        if (mOsArgvLast == environ[i]) {
            size = strlen(environ[i]) + 1;
            mOsArgvLast = environ[i] + size;

            misc::Cpystrn(p, (u_char *) environ[i], size);
            environ[i] = (char *) p;    //转化成char
            p += size;
        }
    }

    // 是原始argv和environ的总体大小。去除结尾一个NULL字符
    mOsArgvLast--;     
    return 0;
}

void wProcTitle::Setproctitle(const char *title, const char *pretitle) {
    // 标题
    u_char pre[512];
    if (pretitle == NULL) {
        Cpystrn(pre, (u_char *) "hnet: ", sizeof(pre));
    } else {
        Cpystrn(pre, (u_char *) pretitle, sizeof(pre));
    }

    mOsArgv[1] = NULL;
    u_char* p = Cpystrn((u_char *) mOsArgv[0], pre, mOsArgvLast - mOsArgv[0]);
    p = Cpystrn(p, (u_char *) title, mOsArgvLast - (char *) p);

    //在原始argv和environ的连续内存中，将修改了的进程名字之外的内存全部清零
    if(mOsArgvLast - (char *) p) {
        memset(p, SETPROCTITLE_PAD, mOsArgvLast - (char *) p);
    }
}

}   // namespace hnet
