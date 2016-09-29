
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wProcTitle.h"
#include "wMisc.h"

namespace hnet {

wProcTitle::wProcTitle(int argc, const char *argv[]) : mArgc(argc), mOsArgv(argv), mOsEnv(environ) {
    mOsArgvLast = argv[0];
    SaveArgv();
}

wProcTitle::~wProcTitle() {
    for (int i = 0; i < mArgc; ++i) {
        SAFE_DELETE_VEC(mArgv[i]);
    }
    SAFE_DELETE_VEC(mArgv);
}

wStatus wProcTitle::SaveArgv() {
    // 初始化argv内存空间
    size_t size = 0;
    SAFE_NEW_VEC(mArgc, char*, mArgv);
    if (mArgv == NULL) {
	   return mStatus = wStatus::IOError("wProcTitle::SaveArgv", "new failed");
    }
    for(int i = 0; i < mArgc; ++i) {
        // 包含\0结尾
        size = strlen(mOsArgv[i]) + 1;
        SAFE_NEW_VEC(size, char, mArgv[i]);
    	if (mArgv[i] == NULL) {
    	    return mStatus = wStatus::IOError("wProcTitle::SaveArgv", "new failed");
    	}
        memcpy(mArgv[i], mOsArgv[i], size);
    }
    return mStatus;
}

wStatus wProcTitle::InitSetproctitle() {
    // environ字符串总长度
    size_t size = 0;
    for (int i = 0; environ[i]; i++) {
        size += strlen(environ[i]) + 1;
    }

    u_char* p;
    SAFE_NEW_VEC(size, u_char, p);
    if (p == NULL) {
        return mStatus = wStatus::IOError("wProcTitle::InitSetproctitle", "new failed");
    }

    // argv字符总长度
    for (int i = 0; mOsArgv[i]; i++) {
        if (mOsArgvLast == mOsArgv[i]) {
            mOsArgvLast = mOsArgv[i] + strlen(mOsArgv[i]) + 1;
        }
    }

    // 移动**environ到堆上
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
    return mStatus;
}

wStatus wProcTitle::Setproctitle(const char *title, const char *pretitle) {
    // 标题
    u_char pre[512];
    if (pretitle == NULL) {
        misc::Cpystrn(pre, (u_char *) "hnet: ", sizeof(pre));
    } else {
        misc::Cpystrn(pre, (u_char *) pretitle, sizeof(pre));
    }

    mOsArgv[1] = NULL;
    u_char* p = misc::Cpystrn((u_char *) mOsArgv[0], pre, mOsArgvLast - mOsArgv[0]);
    p = misc::Cpystrn(p, (u_char *) title, mOsArgvLast - (char *) p);

    //在原始argv和environ的连续内存中，将修改了的进程名字之外的内存全部清零
    if(mOsArgvLast - (char *) p) {
        memset(p, kProcTitlePad, mOsArgvLast - (char *) p);
    }
    return mStatus;
}

}   // namespace hnet
