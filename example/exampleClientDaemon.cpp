
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wMisc.h"
#include "wLog.h"
#include "wTcpTask.h"
#include "wConfig.h"
#include "wMultiClient.h"
#include "exampleCmd.h"

using namespace hnet;

class ExampleTask : public wTcpTask {
public:
	ExampleTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		On(CMD_EXAMPLE_REQ, EXAMPLE_REQ_ECHO, &ExampleTask::ExampleEcho, this);
	}
	int ExampleEcho(struct Request_t *request);
};

int ExampleTask::ExampleEcho(struct Request_t *request) {
	struct ExampleReqEcho_t* cmd = reinterpret_cast<struct ExampleReqEcho_t*>(request->mBuf);

	std::cout << "receive from client：" << cmd->mCmd << std::endl;

	// 异步发送
	AsyncSend(request->mBuf, request->mLen);
	return 0;
}

class ExampleClient : public wMultiClient {
	virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr, int type = 0) {
	    SAFE_NEW(ExampleTask(sock, type), *ptr);
	    if (*ptr == NULL) {
	        return wStatus::IOError("ExampleClient::NewTcpTask", "new failed");
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

	std::string host;
    int16_t port = 0;
    if (!config->GetConf("host", &host) || !config->GetConf("port", &port)) {
    	return -1;
    }

    ExampleClient* client;
	SAFE_NEW(ExampleClient, client);
	if (client == NULL) {
		return -1;
	}

	// 发送command
    const char* str = "hello hnet!";
    struct ExampleReqEcho_t cmd;
    size_t l = strlen(str) + 1;
    memcpy(cmd.mCmd, str, l);
    cmd.mLen = static_cast<uint8_t>(l);

	s = client->PrepareStart();
	if (s.Ok()) {
		int type = 1;
		s = client->AddConnect(type, host, port);
		if (!s.Ok()) {
			std::cout << "add connect:" << s.ToString() << std::endl;
			return -1;
		}

		wStatus s = client->Start();
		std::cout << "client start:" << s.ToString() << std::endl;
	} else {
		std::cout << "client prepare start:" << s.ToString() << std::endl;
		return -1;
	}

	LOG_SHUTDOWN_ALL();
	return 0;
}
