
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wConfig.h"
#include "wSingleClient.h"
#include "exampleCmd.h"

using namespace hnet;

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

	std::string host;
    int16_t port = 0;
    if (!config->GetConf("host", &host) || !config->GetConf("port", &port)) {
    	return -1;
    }

	wSingleClient *client;
	SAFE_NEW(wSingleClient, client);
    if (client == NULL) {
        return -1;
    }

    // 连接
    s = client->Connect(host, port);
    if (!s.Ok()) {
    	std::cout << "client connect failed" << s.ToString() << std::endl;
    	return -1;
    }

    // 发送command
    const char* str = "hello hnet!";
    struct ExampleReqEcho_t cmd;
    size_t l = strlen(str) + 1;
    memcpy(cmd.mCmd, str, l);
    cmd.mLen = static_cast<uint8_t>(l);

	ssize_t size;
	s = client->SyncSend(reinterpret_cast<char *>(&cmd), sizeof(cmd), &size);
	if (!s.Ok()) {
		std::cout << "client send failed" << s.ToString() << std::endl;
		return -1;
	}

	s = client->SyncRecv(reinterpret_cast<char *>(&cmd), sizeof(cmd), &size);
	if (!s.Ok()) {
		std::cout << "client receive failed" << s.ToString() << std::endl;
		return -1;
	}

	std::cout << "receive from server：" << cmd.mCmd << std::endl;
	return 0;
}
