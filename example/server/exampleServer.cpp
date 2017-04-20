
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

#ifdef _USE_PROTOBUF_
#include "example.pb.h"
#else
#include "exampleCmd.h"
#endif

using namespace hnet;

// worker进程通信客户端
class ExampleChannelTask : public wChannelTask {
public:
	ExampleChannelTask(wSocket *socket, wMaster *master, int32_t type = 0) : wChannelTask(socket, master, type) {
#ifdef _USE_PROTOBUF_
		On("example.ExampleEchoReq", &ExampleChannelTask::ExampleEchoReq, this);
#else
		On(CMD_EXAMPLE_REQ, EXAMPLE_REQ_ECHO, &ExampleChannelTask::ExampleEchoReq, this);
#endif
	}
	int ExampleEchoReq(struct Request_t *request);
};

int ExampleChannelTask::ExampleEchoReq(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);
	std::cout << "channel receive:" << req.cmd() << "|" << getpid() << std::endl;
#else
	ExampleReqEcho_t* req = reinterpret_cast<ExampleReqEcho_t*>(request->mBuf);
	std::cout << "channel receive:" << req->mCmd << "|" << getpid() << std::endl;
#endif
	return 0;
}

// TCP客户端类
class ExampleTcpTask : public wTcpTask {
public:
	ExampleTcpTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		// 多播事件注册
#ifdef _USE_PROTOBUF_
		On("example.ExampleEchoReq", &ExampleTcpTask::ExampleEchoReq, this);
		On("example.ExampleEchoReq", &ExampleTcpTask::ExampleEchoChannel, this);
#else
		On(CMD_EXAMPLE_REQ, EXAMPLE_REQ_ECHO, &ExampleTcpTask::ExampleEchoReq, this);
		On(CMD_EXAMPLE_REQ, EXAMPLE_REQ_ECHO, &ExampleTcpTask::ExampleEchoChannel, this);		
#endif
	}
	int ExampleEchoReq(struct Request_t *request);
	int ExampleEchoChannel(struct Request_t *request);
};

int ExampleTcpTask::ExampleEchoReq(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);
	std::cout << "tcp receive 1:" << req.cmd() << std::endl;
#else
	ExampleReqEcho_t* req = reinterpret_cast<ExampleReqEcho_t*>(request->mBuf);
	std::cout << "tcp receive 1:" << req->mCmd << std::endl;
#endif

	// 响应客户端
#ifdef _USE_PROTOBUF_
	example::ExampleEchoRes res;
	res.set_ret(1);
	res.set_cmd("return:" + req.cmd());
	AsyncSend(&res);
#else
	ExampleResEcho_t res;
	res.mRet = 1;
	memcpy(res.mCmd, "return:", sizeof("return:"));
	memcpy(res.mCmd + sizeof("return:"), req->mCmd, req->mLen);
	AsyncSend(reinterpret_cast<char*>(&res), sizeof(res));
#endif
	return 0;
}

int ExampleTcpTask::ExampleEchoChannel(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
	req.ParseFromArray(request->mBuf, request->mLen);
	std::cout << "tcp receive 2:" << req.cmd() << std::endl;
	// 发送给其他worker进程
	SyncWorker(&req);
#else
	ExampleReqEcho_t* req = reinterpret_cast<ExampleReqEcho_t*>(request->mBuf);
	std::cout << "tcp receive 2:" << req->mCmd << std::endl;
	// 发送给其他worker进程
	SyncWorker(request->mBuf, request->mLen);
#endif
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
	if (misc::SetBinPath() == -1) {
		std::cout << "set bin path failed" << std::endl;
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
				// Master-Worker方式开启服务器
				master->MasterStart();
			} else {
				return -1;
			}
	    }
	}

	return 0;
}
