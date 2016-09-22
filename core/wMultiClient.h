
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MULTI_CLIENT_H_
#define _W_MULTI_CLIENT_H_

#include <algorithm>
#include <map>
#include <vector>
#include <sys/epoll.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

const int32_t kReserveType = 0;

class wEnv;
class wTimer;
class wClient;

class wMultiClient : private wNoncopyable {
public:
	wMultiClient();
	virtual ~wMultiClient();

	wStatus MountClient(int32_t type, std::string ipaddr, uint16_t port, std::string protocol = "TCP");
	wStatus UnmountClient();
	
	wStatus Prepare();
	wStatus Start(bool deamon = true);
    
    // 异步广播消息
    wStatus Broadcast(const char *cmd, int len, int32_t type = kReserveType);

    virtual wStatus PrepareRun() {
        return wStatus::IOError("wMultiClient::PrepareRun, server will be exit", "method should be inherit");
    }

    virtual wStatus Run() {
        return wStatus::IOError("wMultiClient::Run, server will be exit", "method should be inherit");
    }
	
	virtual void CheckTimeout();

protected:
	// 异步发送消息
    wStatus Send(wClient *client, const char *cmd, int len);
	void CheckTimer();

    wStatus Recv();
    wStatus InitEpoll();
    wStatus AddClient(int32_t type, wClient* client, int ev = EPOLLIN, int op = EPOLL_CTL_ADD, bool newconn = true);
    wStatus AddToClientPool(int32_t type, wClient* client);

    wStatus RemoveClient(wClient* client, std::vector<wClient*>::iterator* iter = NULL);
    std::vector<wClient*>::iterator RemoveClientPool(int32_t type, wClient* client);

    wStatus mStatus;
    wEnv *mEnv;
	
	uint64_t mLatestTm;	// 服务器当前时间 微妙
    wTimer mHeartbeatTimer;	// 定时器
    bool mCheckSwitch;	// 心跳开关，默认关闭。强烈建议移动互联网环境下打开，而非依赖keepalive机制保活

    int32_t mEpollFD;
    uint64_t mTimeout;
	
	std::map<int32_t, std::vector<*wClient> > mClientPool;
};

}	// namespace hnet

#endif