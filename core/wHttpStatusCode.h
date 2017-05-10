
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_HTTP_STATUS_CODE_H_
#define _W_HTTP_STATUS_CODE_H_

#include <map>
#include "wCore.h"

namespace hnet {

struct HttpStatusCode_t {
	const char	code[4];
	const char	status[64];
};

extern std::map<const std::string, const std::string> gHttpStatusCode;

const std::string& HttpStatus(std::string& code);

}	// namespace hnet

#endif
