
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_DAEMON_H_
#define _W_DAEMON_H_

#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

class wDaemon : private wNoncopyable {
public:
	int Start(const std::string& lock_path, const char *prefix = NULL);

private:
	std::string mFilename;
};

}	// namespce hnet

#endif
