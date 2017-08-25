
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wMisc.h"
#include "wDaemon.h"
#include "wTcpTask.h"
#include "wMultiClient.h"
#include "exampleCmd.h"

#ifdef _USE_PROTOBUF_
#include "example.pb.h"
#endif

using namespace hnet;

class ExampleTask : public wTcpTask {
public:
	ExampleTask(wSocket *socket, int32_t type = 0) : wTcpTask(socket, type) {
#ifdef _USE_PROTOBUF_
		On("example.ExampleEchoRes", &ExampleTask::ExampleEchoRes, this);
#else
		On(example::CMD_EXAMPLE_RES, example::EXAMPLE_RES_ECHO, &ExampleTask::ExampleEchoRes, this);
#endif
	}
    virtual int ReConnect();
	
	int ExampleEchoRes(struct Request_t *request);
};

int ExampleTask::ReConnect() {

std::cout << "ReConnect" << std::endl;

// 重连事件
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
#else
	example::ExampleReqEcho_t req;
#endif
	req.set_cmd("hello hnet~");

#ifdef _USE_PROTOBUF_
	AsyncSend(&req);
#else
	AsyncSend(reinterpret_cast<char*>(&req), sizeof(req));
#endif

	return 0;
}

int ExampleTask::ExampleEchoRes(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoRes res;
#else
	example::ExampleResEcho_t res;
#endif
	res.ParseFromArray(request->mBuf, request->mLen);
	std::cout << res.cmd() << "|" << res.ret() << std::endl;

// 循环请求
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
#else
	example::ExampleReqEcho_t req;
#endif
	req.set_cmd("hello hnet~");

#ifdef _USE_PROTOBUF_
	AsyncSend(&req);
#else
	AsyncSend(reinterpret_cast<char*>(&req), sizeof(req));
#endif

	return 0;
}

class ExampleClient : public wMultiClient {
public:
	ExampleClient(wConfig* config, wServer* server = NULL) : wMultiClient(config, server, true) { }

	virtual int NewTcpTask(wSocket* sock, wTask** ptr, int type = 0) {
	    HNET_NEW(ExampleTask(sock, type), *ptr);
	    if (!*ptr) {
	    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "ExampleClient::NewTcpTask new() failed", "");
	    	return -1;
	    }
	    return 0;
	}

	virtual int PrepareRun() {
	    std::string host;
	    int16_t port = 0;

	    wConfig* config = Config<wConfig*>();
	    if (!config || !config->GetConf("host", &host) || !config->GetConf("port", &port)) {
	    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "ExampleClient::PrepareRun () failed", "");
	    	return -1;
	    }

	    // 服务器key，每个key可挂载连接多个服务器
		int type = 1;

		// 连接服务器
		int ret = AddConnect(type, host, port);
		if (ret == -1) {
			std::cout << "connect error" << std::endl;
		}
		return ret;
	}
};

int main(int argc, char *argv[]) {
	// 设置运行目录
	if (misc::SetBinPath() == -1) {
		std::cout << "set bin path failed" << std::endl;
		return -1;
	}

	// 创建配置对象
	wConfig* config;
	HNET_NEW(wConfig, config);
	if (!config) {
		std::cout << "config new failed" << std::endl;
		return -1;
	}

	// 解析命令行
	if (config->GetOption(argc, argv) == -1) {
		std::cout << "get configure failed" << std::endl;
		HNET_DELETE(config);
		return -1;
	}

	// 日志路径
	std::string log_path;
	if (config->GetConf("log_path", &log_path)) {
		soft::SetLogdirPath(log_path);
	}

	// 相对目录路径
	std::string runtime_path;
	if (config->GetConf("runtime_path", &runtime_path)) {
		soft::SetRuntimePath(runtime_path);
	}

	// 版本输出 && 守护进程创建
	bool vn, dn;
	if (config->GetConf("version", &vn) && vn) {
		std::cout << soft::SetSoftName("example clientd -") << soft::GetSoftVer() << std::endl;
		HNET_DELETE(config);
		return -1;
	} else if (config->GetConf("daemon", &dn) && dn) {
		std::string lock_path;
		config->GetConf("lock_path", &lock_path);
		wDaemon daemon;
		if (daemon.Start(lock_path) == -1) {
			std::cout << "create daemon failed" << std::endl;
			HNET_DELETE(config);
			return -1;
		}
	}
	
	// 创建客户端
    ExampleClient* client;
	HNET_NEW(ExampleClient(config), client);
	if (client == NULL) {
		std::cout << "client new failed" << std::endl;
		HNET_DELETE(config);
		return -1;
	}

	// 准备客户端
	int ret = client->PrepareStart();
	if (ret == 0) {
		/** 守护线程方式运行*/
		std::cout << "thread start" << std::endl;
		client->StartThread();

		// 广播发送example::ExampleEchoReq请求到所有连接的服务器
#ifdef _USE_PROTOBUF_
		example::ExampleEchoReq req;
#else
		example::ExampleReqEcho_t req;
#endif
		req.set_cmd("hello hnet~");
		
#ifdef _USE_PROTOBUF_
		client->Broadcast(&req, 1);
#else
		client->Broadcast(reinterpret_cast<char*>(&req), sizeof(req), 1);
#endif
		client->JoinThread();	// 等待连接结束

		std::cout << "thread end" << std::endl;
	} else {
		std::cout << "prepare error" << std::endl;
		ret = -1;
	}
	
	HNET_DELETE(config);
	HNET_DELETE(client);
	return ret;
}
