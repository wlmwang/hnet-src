
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

	// 创建配置对象
	wConfig* config;
	SAFE_NEW(wConfig, config);
	if (config == NULL) {
		return -1;
	}

	wStatus s;

	// 解析命令行
	s = config->GetOption(argc, argv);
	if (!s.Ok()) {
		std::cout << "get configure:" << s.ToString() << std::endl;
		return -1;
	}

	// 版本输出
	bool version;
	if (config->GetConf("version", &version) && version == true) {
		std::cout << soft::SetSoftName("example client -") << soft::GetSoftVer() << std::endl;
		return -1;
	}

	// 命令行-h、-p解析
	std::string host;
    int16_t port = 0;
    if (!config->GetConf("host", &host) || !config->GetConf("port", &port)) {
    	return -1;
    }

    // 创建客户端
	wSingleClient *client;
	SAFE_NEW(wSingleClient, client);
    if (client == NULL) {
        return -1;
    }

    // 连接服务器
    s = client->Connect(host, port);
    if (!s.Ok()) {
    	std::cout << "client connect failed" << s.ToString() << std::endl;
    	return -1;
    }

    ssize_t size;

    // 同步方式发送example::ExampleEchoReq请求
    example::ExampleEchoReq req;
    req.set_cmd("hello hnet~");
	s = client->SyncSend(&req, &size);
	if (!s.Ok()) {
		std::cout << "client send failed" << s.ToString() << std::endl;
		return -1;
	}

	// 同步接受服务器返回
	example::ExampleEchoRes res;
	s = client->SyncRecv(&res, &size);
	if (!s.Ok()) {
		std::cout << "client receive failed" << s.ToString() << std::endl;
		return -1;
	}
	std::cout << res.cmd() << "|" << res.ret() << std::endl;

	return 0;
}
