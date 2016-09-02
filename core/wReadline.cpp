
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wReadline.h"

namespace hnet {

const char *wReadline::mQuitCmd[] = {"Quit", "Exit", "End", "Bye"};
const unsigned char wReadline::mQuitCmdNum = sizeof(wReadline::mQuitCmd) / sizeof(wReadline::mQuitCmd[0]);

wReadline::wReadline() {
	memcpy(mPrompt, "Anny>", 5);
}

wReadline::~wReadline() {
	SAFE_FREE(mLineRead);
}

inline void wReadline::SetPrompt(char* cStr, int iLen) {
	memcpy(mPrompt, cStr, iLen);
}

bool wReadline::SetCompletion(CompletionFunc_t pFunc) {
	if (pFunc != NULL) {
		rl_attempted_completion_function = pFunc;
		return true;
	} else if (mFunc != NULL) {
		rl_attempted_completion_function = mFunc;
		return true;
	}
	return false;
}

char *wReadline::ReadCmdLine() {
	SAFE_FREE(mLineRead);
	mLineRead = readline(mPrompt);

	mStripLine = StripWhite(mLineRead);
	if (mStripLine && *mStripLine) {
		add_history(mStripLine);
	}

	return mStripLine;
}

char *wReadline::StripWhite(char *pOrig) {
	if (NULL == pOrig) return NULL;

	char *pStripHead = pOrig;
	while (isspace(*pStripHead)) {
		pStripHead++;
	}
	if ('\0' == *pStripHead) {
		return pStripHead;
	}

	char *pStripTail = pStripHead + strlen(pStripHead) - 1;
	while (pStripTail > pStripHead && isspace(*pStripTail)) {
		pStripTail--;
	}
	
	*(++pStripTail) = '\0';

	return pStripHead;
}

bool wReadline::IsUserQuitCmd(char *pCmd) {
	for (unsigned char ucQuitCmdIdx = 0; ucQuitCmdIdx < wReadline::mQuitCmdNum; ucQuitCmdIdx++) {
		if (!strcasecmp(pCmd, wReadline::mQuitCmd[ucQuitCmdIdx])) return true;
	}

	return false;
}

}	// namespace hnet

