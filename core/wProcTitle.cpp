
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wProcTitle.h"
#include "wMisc.h"
#include "wLogger.h"

extern char** environ;

namespace hnet {

wProcTitle::wProcTitle() {
    while (environ[mOsEnvc]) {
        mOsEnvc++;
    }
    mOsEnv = environ;
}

wProcTitle::~wProcTitle() {
    for (int i = 0; i < mOsArgc; ++i) {
        HNET_DELETE_VEC(mArgv[i]);
    }
    HNET_DELETE_VEC(mArgv);

    for (int i = 0; i < mOsEnvc; ++i) {
        HNET_DELETE_VEC(mEnv[i]);
    }
    HNET_DELETE_VEC(mEnv);
}

int wProcTitle::SaveArgv(int argc, char* argv[]) {
	mOsArgc = argc;
	mOsArgv = argv;

	size_t size = 0;

    // 移动**argv到堆上mArgv
    HNET_NEW_VEC(mOsArgc + 1, char*, mArgv);    // 结尾指针NULL
    for (int i = 0; i < mOsArgc; ++i) {
        size = strlen(argv[i]) + 1; // 包含\0结尾
        HNET_NEW_VEC(size, char, mArgv[i]);

        memcpy(mArgv[i], argv[i], size);
    }
    mArgv[mOsArgc] = NULL;

    HNET_NEW_VEC(mOsEnvc + 1, char*, mEnv);
    for (int i = 0; i < mOsEnvc; ++i) {
        size = strlen(environ[i]) + 1;  // 包含\0结尾
        HNET_NEW_VEC(size, char, mEnv[i]);

        memcpy(mEnv[i], environ[i], size);
    }
    mEnv[mOsEnvc] = NULL;
    return 0;
}

int wProcTitle::Setproctitle(const char *title, const char *pretitle, bool attach) {
    if (pretitle == NULL) {
    	pretitle = soft::GetSoftName().c_str();
    }
	mOsArgv[1] = NULL;

	size_t size = 1024;
	char* p = mOsArgv[0];

    memcpy(p, pretitle, strlen(pretitle) + 1);
    strncat(p, title, size -= strlen(pretitle));
    size -= strlen(title);

    const char* blank = " ";
    if (attach) {
        for (int i = 0; i < mOsArgc; i++) {
            strncat(p, blank, size -= strlen(blank));
            strncat(p, mArgv[i], size -= strlen(mArgv[i]));
        }
    }
    return 0;
}

}   // namespace hnet
