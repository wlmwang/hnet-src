
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wProcTitle.h"
#include "wMisc.h"

extern char **environ;

namespace hnet {

wProcTitle::wProcTitle(int argc, const char* argv[]) {
    SaveArgv(argc, argv);
}

wProcTitle::~wProcTitle() {
    for (int i = 0; i < mArgc; ++i) {
        SAFE_DELETE_VEC(mArgv[i]);
    }
    SAFE_DELETE_VEC(mArgv);

    for (int i = 0; i < mNumEnv; ++i) {
        SAFE_DELETE_VEC(mEnv[i]);
    }
    SAFE_DELETE_VEC(mEnv);
}

const wStatus& wProcTitle::SaveArgv(int argc, const char* argv[]) {
	mArgc = argc;
	mOsArgv = argv;
	mNumEnv = 0;

	size_t size = 0;

    // 移动**argv到堆上mArgv
    SAFE_NEW_VEC(mArgc, char*, mArgv);
    if (mArgv == NULL) {
	   return mStatus = wStatus::IOError("wProcTitle::SaveArgv", "new failed");
    }
    for (int i = 0; i < mArgc; ++i) {
        // 包含\0结尾
        size = strlen(argv[i]) + 1;
        SAFE_NEW_VEC(size, char, mArgv[i]);
    	if (mArgv[i] == NULL) {
    	    mStatus = wStatus::IOError("wProcTitle::SaveArgv", "new failed");
    	    break;
    	}
        memcpy(mArgv[i], argv[i], size);
    }

    // 移动**environ到堆上mEnv
    while (environ[mNumEnv]) { mNumEnv++;}
    SAFE_NEW_VEC(mNumEnv, char*, mEnv);
    if (mEnv == NULL) {
	   return mStatus = wStatus::IOError("wProcTitle::SaveArgv", "new failed");
    }
    for (int i = 0; i < mNumEnv; ++i) {
        // 包含\0结尾
        size = strlen(environ[i]) + 1;
        SAFE_NEW_VEC(size, char, mEnv[i]);
    	if (mEnv[i] == NULL) {
    	    mStatus = wStatus::IOError("wProcTitle::SaveArgv", "new failed");
    	    break;
    	}
        memcpy(mEnv[i], environ[i], size);
    }
    environ = mEnv;

    return mStatus.Clear();
}

const wStatus& wProcTitle::Setproctitle(const char *title, const char *pretitle) {
    if (pretitle == NULL) {
    	pretitle = "HNET: ";
    }
	mOsArgv[1] = NULL;

	size_t size = 1024;
	char* p = const_cast<char*>(mOsArgv[0]);
    p = misc::Cpystrn(p, pretitle, size);
    p = misc::Cpystrn(p, title, size -= strlen(pretitle));

    size -= strlen(title);
    for (int i = 0; i < mArgc; i++) {
    	p = misc::Cpystrn(p, " ", size -= 1);
    	p = misc::Cpystrn(p, mArgv[i], size -= strlen(mArgv[i]));
    }
    return mStatus.Clear();
}

}   // namespace hnet
