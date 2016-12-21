
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

// worker进程通信客户端
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

// TCP客户端类
class ExampleTcpTask : public wTcpTask {
public:
	ExampleTcpTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		// 多播事件注册
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

	// 响应客户端
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

	virtual const wStatus& NewTcpTask(wSocket* sock, wTask** ptr) {
	    SAFE_NEW(ExampleTcpTask(sock, Shard(sock)), *ptr);
	    if (*ptr == NULL) {
			return mStatus = wStatus::IOError("ExampleServer::NewTcpTask", "ExampleTask new failed");
	    }
	    return mStatus;
	}

	virtual const wStatus& NewChannelTask(wSocket* sock, wTask** ptr) {
		SAFE_NEW(ExampleChannelTask(sock, mMaster, Shard(sock)), *ptr);
	    if (*ptr == NULL) {
			return mStatus = wStatus::IOError("ExampleServer::NewChannelTask", "new failed");
	    }
	    return mStatus;
	}
};

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
		return -1;
	}

	// 版本输出 && 守护进程创建
	bool version, daemon;
	if (config->GetConf("version", &version) && version == true) {
		std::cout << soft::SetSoftName("example server -") << soft::GetSoftVer() << std::endl;
		return -1;
	} else if (config->GetConf("daemon", &daemon) && daemon == true) {
		std::string lock_path;
		config->GetConf("lock_path", &lock_path);
		if (!misc::InitDaemon(lock_path).Ok()) {
			std::cout << "create daemon failed" << std::endl;
			return -1;
		}
	}

	// 创建服务器对象
	ExampleServer* server;
	SAFE_NEW(ExampleServer(config), server);
	if (server == NULL) {
		return -1;
	}

	// 创建master对象
	wMaster* master;
	SAFE_NEW(wMaster("EXAMPLE", server), master);
	if (master != NULL) {
		// 接受命令信号
	    std::string signal;
	    if (config->GetConf("signal", &signal) && signal.size() > 0) {
	    	if (master->SignalProcess(signal).Ok()) {
	    		return 0;
	    	} else {
	    		return -1;
	    	}
	    } else {
	    	// 准备服务器
			s = master->PrepareStart();
			if (s.Ok()) {
				// single方式开启服务器
				//master->SingleStart();

				// Master-Worker方式开启服务器
				master->MasterStart();
			} else {
				return -1;
			}
	    }
	}

	return 0;
}
