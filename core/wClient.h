
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MTCP_CLIENT_H_
#define _W_MTCP_CLIENT_H_

#include <algorithm>
#include <map>
#include <vector>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "wCore.h"
#include "wStatus.h"

namespace hnet {

class wClient {
public:
	wClient();
	virtual ~wClient();
	
};

}	// namespace hnet

#endif