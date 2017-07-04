
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wConfig.h"
#include "wSingleClient.h"
#include "exampleCmd.h"

#ifdef _USE_PROTOBUF_
#include "example.pb.h"
#endif

using namespace hnet;

int main(int argc, char *argv[]) {
	// 设置运行目录
	if (misc::SetBinPath() == -1) {
		std::cout << "set bin path failed" << std::endl;
		return -1;
	}

	// 创建配置对象
	wConfig* config;
	SAFE_NEW(wConfig, config);
	if (!config) {
		std::cout << "config new failed" << std::endl;
		return -1;
	}
	
	// 解析命令行
	if (config->GetOption(argc, argv) == -1) {
		std::cout << "get configure failed" << std::endl;
		SAFE_DELETE(config);
		return -1;
	}

	// 版本输出
	bool version;
	if (config->GetConf("version", &version) && version) {
		std::cout << soft::SetSoftName("example client -") << soft::GetSoftVer() << std::endl;
		SAFE_DELETE(config);
		return -1;
	}

	// 命令行-h、-p解析
	std::string host;
    int16_t port = 0;
    if (!config->GetConf("host", &host) || !config->GetConf("port", &port)) {
    	std::cout << "host or port error" << std::endl;
    	SAFE_DELETE(config);
    	return -1;
    }

    // 创建客户端
	wSingleClient *client;
	SAFE_NEW(wSingleClient, client);
    if (!client) {
    	std::cout << "client new failed" << std::endl;
    	SAFE_DELETE(config);
        return -1;
    }
    
    // 连接服务器
    int ret = client->Connect(host, port);
    if (ret == -1) {
    	std::cout << "client connect failed" << std::endl;
    	SAFE_DELETE(config);
    	SAFE_DELETE(client);
    	return -1;
    }

    ssize_t size;

    // 同步方式发送example::ExampleEchoReq请求
#ifdef _USE_PROTOBUF_
    example::ExampleEchoReq req;
#else
	example::ExampleReqEcho_t req;
#endif
    req.set_cmd("hello hnet~");

#ifdef _USE_PROTOBUF_
	ret = client->SyncSend(&req, &size);
#else
	ret = client->SyncSend(reinterpret_cast<char*>(&req), sizeof(req), &size);
#endif

	if (ret == -1) {
		std::cout << "client send failed" << std::endl;
    	SAFE_DELETE(config);
    	SAFE_DELETE(client);
		return -1;
	}

	// 同步接受服务器返回
#ifdef _USE_PROTOBUF_
	example::ExampleEchoRes res;
	ret = client->SyncRecv(&res, &size);

	if (ret == -1) {
		std::cout << "client receive failed" << s.ToString() << std::endl;
    	SAFE_DELETE(config);
    	SAFE_DELETE(client);
		return -1;
	}
#else
	example::ExampleResEcho_t res;
	ret = client->SyncRecv(reinterpret_cast<char*>(&res), &size, sizeof(res));

	if (ret == -1) {
		std::cout << "client receive failed" << std::endl;
    	SAFE_DELETE(config);
    	SAFE_DELETE(client);
		return -1;
	}
#endif
	std::cout << res.cmd() << "|" << res.ret() << std::endl;
    SAFE_DELETE(config);
    SAFE_DELETE(client);
    
	return 0;
}
