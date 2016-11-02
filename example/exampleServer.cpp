
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wMisc.h"
#include "wTcpTask.h"
#include "wConfig.h"
#include "wServer.h"
#include "wMaster.h"
#include "example.pb.h"

using namespace hnet;

class ExampleTask : public wTcpTask {
public:
	ExampleTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		On("example.ExampleEchoReq", &ExampleTask::ExampleEchoReq, this);
	}
	int ExampleEchoReq(struct Request_t *request);
};

int ExampleTask::ExampleEchoReq(struct Request_t *request) {
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);

	std::cout << "receive from client：" << req.cmd() << std::endl;

	// 响应
	example::ExampleEchoRes res;
	res.set_cmd("echo:" + req.cmd());
	res.set_ret(1);
	AsyncSend(&res);
	return 0;
}

class ExampleServer : public wServer {
public:
	virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr) {
	    SAFE_NEW(ExampleTask(sock, Shard(sock)), *ptr);
	    if (*ptr == NULL) {
			return wStatus::IOError("ExampleServer::NewTcpTask", "ExampleTask new failed");
	    }
	    return mStatus;
	}
};

int main(int argc, const char *argv[]) {
	wConfig* config;
	SAFE_NEW(wConfig, config);
	if (config == NULL) {
		return -1;
	}

	wStatus s;
	s = config->GetOption(argc, argv);
	if (!s.Ok()) {
		std::cout << "get configure:" << s.ToString() << std::endl;
		return -1;
	}

	bool version;
	if (config->GetConf("version", &version) && version == true) {
		std::cout << kSoftwareName << kSoftwareVer << std::endl;
		return -1;
	}
	bool daemon;
	if (config->GetConf("daemon", &daemon) && daemon == true) {
		std::string lock_path;
		config->GetConf("lock_path", &lock_path);
		if (!misc::InitDaemon(lock_path).Ok()) {
			std::cout << "create daemon failed" << std::endl;
			return -1;
		}
	}

	ExampleServer* server;
	SAFE_NEW(ExampleServer, server);
	if (server == NULL) {
		return -1;
	}

	wMaster* master;
	SAFE_NEW(wMaster("EXAMPLE", server, config), master);
	if (master != NULL) {
		s = master->PrepareStart();
		if (s.Ok()) {
			wStatus s = master->MasterStart();
			std::cout << "master start:" << s.ToString() << std::endl;
		} else {
			std::cout << "master prepare start:" << s.ToString() << std::endl;
			return -1;
		}
	}

	return 0;
}
