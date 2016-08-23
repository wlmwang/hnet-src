
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ASSERT_H_
#define _W_ASSERT_H_

#include "wCore.h"
#include "wLog.h"

#define W_ASSERT(a, b) \
	if(!(a)) \
	{  \
		LOG_ERROR(ELOG_KEY, "[system] ASSERT %s failed, %s, %s, %d", #a, __FILE__, __FUNCTION__, __LINE__); \
		b; \
	}

#endif
