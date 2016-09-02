
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_READLINE_H_
#define _W_READLINE_H_

#include <readline/readline.h>
#include <readline/history.h>
#include "wCore.h"

namespace hnet {
class wReadline {
public:
	typedef char** (*CompletionFunc_t)(const char *pText, int iStart, int iEnd);	//Tab键能补齐的函数类型
	wReadline();
	virtual ~wReadline();
	
	char *ReadCmdLine();
	virtual char *StripWhite(char *pOrig);
	virtual bool IsUserQuitCmd(char *pCmd);

	bool SetCompletion(CompletionFunc_t pFunc);
	void SetPrompt(char* cStr, int iLen);

protected:
	char mPrompt[32] {'\0'};
	char *mLineRead {NULL};
	char *mStripLine {NULL};

	CompletionFunc_t mFunc {NULL};
	static const char *mQuitCmd[] {NULL};
	static const unsigned char mQuitCmdNum {0};
};

}	// namespace hnet

#endif
