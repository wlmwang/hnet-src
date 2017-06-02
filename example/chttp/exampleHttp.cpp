
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wMisc.h"
#include "wConfig.h"
#include "wSingleClient.h"
#include "exampleCmd.h"

using namespace hnet;

int main(int argc, const char *argv[]) {
	// 设置运行目录
	if (misc::SetBinPath() == -1) {
		std::cout << "set bin path failed" << std::endl;
		return -1;
	}

	// 创建配置对象
	wConfig* config;
	SAFE_NEW(wConfig, config);
	if (config == NULL) {
		return -1;
	}

	// 解析命令行
	if (config->GetOption(argc, argv) == -1) {
		std::cout << "get configure failed" << std::endl;
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

    wStatus s;
    // 连接服务器
    s = client->Connect(host, port, "HTTP");
    if (!s.Ok()) {
    	std::cout << "client connect failed" << s.ToString() << std::endl;
    	return -1;
    }

	// 发送
	std::string res;
	std::map<std::string, std::string> header;
	header.insert(std::make_pair("Content-Type", "text/html; charset=UTF-8"));
    s = client->HttpGet("?cmd=50&para=0", header, res);
    if (!s.Ok()) {
    	std::cout << "http get failed:" << s.ToString() << std::endl;
    }
    usleep(1000);

    std::cout << "response:" << res << std::endl;
	return 0;
}
