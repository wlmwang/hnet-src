
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wMisc.h"
#include "wChannelTask.h"
#include "wTcpTask.h"
#include "wConfig.h"
#include "wServer.h"
#include "wMaster.h"
#include "example.pb.h"

using namespace hnet;

class ExampleChannelTask : public wChannelTask {
public:
	ExampleChannelTask(wSocket *socket, wMaster *master, int32_t type = 0) : wChannelTask(socket, master, type) {
		On("example.ExampleEchoReq", &ExampleChannelTask::ExampleEchoReq, this);
	}
	int ExampleEchoReq(struct Request_t *request);
};

int ExampleChannelTask::ExampleEchoReq(struct Request_t *request) {
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);

	std::cout << "channel receive:" << req.cmd() << "|" << getpid() << std::endl;
	return 0;
}

class ExampleTcpTask : public wTcpTask {
public:
	ExampleTcpTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		On("example.ExampleEchoReq", &ExampleTcpTask::ExampleEchoReq, this);
		On("example.ExampleEchoReq", &ExampleTcpTask::ExampleEchoChannel, this);
	}
	int ExampleEchoReq(struct Request_t *request);
	int ExampleEchoChannel(struct Request_t *request);
};

int ExampleTcpTask::ExampleEchoReq(struct Request_t *request) {
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);

	std::cout << "tcp receive 1:" << req.cmd() << std::endl;

	// 响应
	example::ExampleEchoRes res;
	res.set_cmd("return:" + req.cmd());
	res.set_ret(1);
	AsyncSend(&res);
	return 0;
}

int ExampleTcpTask::ExampleEchoChannel(struct Request_t *request) {
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);

	std::cout << "tcp receive 2:" << req.cmd() << std::endl;

	// 发送给其他worker进程
	SyncWorker(&req);
	return 0;
}

class ExampleServer : public wServer {
public:
	ExampleServer(wConfig* config) : wServer(config) { }

	virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr) {
	    SAFE_NEW(ExampleTcpTask(sock, Shard(sock)), *ptr);
	    if (*ptr == NULL) {
			return wStatus::IOError("ExampleServer::NewTcpTask", "ExampleTask new failed");
	    }
	    return mStatus;
	}

	virtual wStatus NewChannelTask(wSocket* sock, wTask** ptr) {
		SAFE_NEW(ExampleChannelTask(sock, mMaster, Shard(sock)), *ptr);
	    if (*ptr == NULL) {
			return wStatus::IOError("ExampleServer::NewChannelTask", "new failed");
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
		return -1;
	}

	bool version, daemon;
	if (config->GetConf("version", &version) && version == true) {
		std::cout << kSoftwareName << kSoftwareVer << std::endl;
		return -1;
	} else if (config->GetConf("daemon", &daemon) && daemon == true) {
		std::string lock_path;
		config->GetConf("lock_path", &lock_path);
		s = misc::InitDaemon(lock_path);
		if (!s.Ok()) {
			return -1;
		}
	}

	ExampleServer* server;
	SAFE_NEW(ExampleServer(config), server);
	if (server == NULL) {
		return -1;
	}

	wMaster* master;
	SAFE_NEW(wMaster("EXAMPLE", server), master);
	if (master != NULL) {
	    std::string signal;
	    if (config->GetConf("signal", &signal) && signal.size() > 0) {
	    	if (master->SignalProcess(signal).Ok()) {
	    		return 0;
	    	} else {
	    		return -1;
	    	}
	    } else {
			s = master->PrepareStart();
			if (s.Ok()) {
				wStatus s = master->MasterStart();
			} else {
				return -1;
			}
	    }
	}

	return 0;
}
