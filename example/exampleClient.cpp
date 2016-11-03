
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wConfig.h"
#include "wSingleClient.h"
#include "example.pb.h"

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
    ssize_t size;

    // 发送protobuf
    example::ExampleEchoReq req;
    req.set_cmd("hello hnet~");
	s = client->SyncSend(&req, &size);
	if (!s.Ok()) {
		std::cout << "client send failed" << s.ToString() << std::endl;
		return -1;
	}

	example::ExampleEchoRes res;
	s = client->SyncRecv(&res, &size);
	if (!s.Ok()) {
		std::cout << "client receive failed" << s.ToString() << std::endl;
		return -1;
	}

	std::cout << res.cmd() << "|" << res.ret() << std::endl;
	return 0;
}
