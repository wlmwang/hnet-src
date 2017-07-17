
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wMisc.h"
#include "wDaemon.h"
#include "wChannelTask.h"
#include "wTcpTask.h"
#include "wHttpTask.h"
#include "wConfig.h"
#include "wServer.h"
#include "wMaster.h"
#include "exampleCmd.h"

#ifdef _USE_PROTOBUF_
#include "example.pb.h"
#endif

using namespace hnet;

// worker进程通信客户端
class ExampleChannelTask : public wChannelTask {
public:
	ExampleChannelTask(wSocket *socket, wMaster *master, int32_t type = 0) : wChannelTask(socket, master, type) {
#ifdef _USE_PROTOBUF_
		On("example.ExampleEchoReq", &ExampleChannelTask::ExampleEchoReq, this);
#else
		On(example::CMD_EXAMPLE_REQ, example::EXAMPLE_REQ_ECHO, &ExampleChannelTask::ExampleEchoReq, this);
#endif
	}
	int ExampleEchoReq(struct Request_t *request);
};

int ExampleChannelTask::ExampleEchoReq(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
#else
	example::ExampleReqEcho_t req;
#endif
	req.ParseFromArray(request->mBuf, request->mLen);
	std::cout << "channel receive:" << req.cmd() << "|" << getpid() << std::endl;
	return 0;
}

// TCP客户端类
class ExampleTcpTask : public wTcpTask {
public:
	ExampleTcpTask(wSocket *socket, int32_t type = 0) : wTcpTask(socket, type) {
		// 多播事件注册
#ifdef _USE_PROTOBUF_
		On("example.ExampleEchoReq", &ExampleTcpTask::ExampleEchoReq, this);
		On("example.ExampleEchoReq", &ExampleTcpTask::ExampleEchoChannel, this);
#else
		On(example::CMD_EXAMPLE_REQ, example::EXAMPLE_REQ_ECHO, &ExampleTcpTask::ExampleEchoReq, this);
		On(example::CMD_EXAMPLE_REQ, example::EXAMPLE_REQ_ECHO, &ExampleTcpTask::ExampleEchoChannel, this);		
#endif
	}
	int ExampleEchoReq(struct Request_t *request);
	int ExampleEchoChannel(struct Request_t *request);
};

int ExampleTcpTask::ExampleEchoReq(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
#else
	example::ExampleReqEcho_t req;
#endif
	req.ParseFromArray(request->mBuf, request->mLen);
	std::cout << "tcp receive 1:" << req.cmd() << std::endl;

	// 响应客户端
#ifdef _USE_PROTOBUF_
	example::ExampleEchoRes res;
#else
	example::ExampleResEcho_t res;
#endif

	res.set_ret(1);
	res.set_cmd("return:" + req.cmd());

#ifdef _USE_PROTOBUF_
	AsyncSend(&res);
#else
	AsyncSend(reinterpret_cast<char*>(&res), sizeof(res));
#endif
	return 0;
}

int ExampleTcpTask::ExampleEchoChannel(struct Request_t *request) {
#ifdef _USE_PROTOBUF_
	example::ExampleEchoReq req;
#else
	example::ExampleReqEcho_t req;
#endif

	req.ParseFromArray(request->mBuf, request->mLen);
	std::cout << "tcp receive 2:" << req.cmd() << std::endl;

	// 同步所有worker进程
#ifdef _USE_PROTOBUF_
	AsyncWorker(&req);
#else
	AsyncWorker(request->mBuf, request->mLen);
#endif
	return 0;
}

class ExampleHttpTask : public wHttpTask {
public:
	ExampleHttpTask(wSocket *socket, int32_t type = 0) : wHttpTask(socket, type) {
		// 多播事件注册
		On(example::CMD_EXAMPLE_REQ, example::EXAMPLE_REQ_ECHO, &ExampleHttpTask::ExampleEchoReq, this);
	}
	int ExampleEchoReq(struct Request_t *request);
};

int ExampleHttpTask::ExampleEchoReq(struct Request_t *request) {

	std::cout << QueryGet("cmd") << " | " << QueryGet("para") << std::endl;
	for (std::map<std::string, std::string>::iterator it = Req().begin(); it != Req().end(); it++) {
		std::cout << "REQ: " << it->first << " = " << it->second << std::endl;
	}

	// 返回
	ResponseSet("Content-Type", "text/html; charset=UTF-8");
	Write("<h1>hnet is work!<h1>");

	for (std::map<std::string, std::string>::iterator it = Res().begin(); it != Res().end(); it++) {
		std::cout << "RES: " << it->first << " = " << it->second << std::endl;
	}
	std::cout << "-------------------" << std::endl;
	return 0;
}

class ExampleServer : public wServer {
public:
	ExampleServer(wConfig* config) : wServer(config) { }

	virtual int NewTcpTask(wSocket* sock, wTask** ptr) {
	    HNET_NEW(ExampleTcpTask(sock), *ptr);
	    if (!*ptr) {
	    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "ExampleServer::NewTcpTask new() failed", "");
	    	return -1;
	    }
	    return 0;
	}
	
	virtual int NewHttpTask(wSocket* sock, wTask** ptr) {
	    HNET_NEW(ExampleHttpTask(sock), *ptr);
	    if (!*ptr) {
	    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "ExampleServer::NewHttpTask new() failed", "");
	    	return -1;
	    }
	    return 0;
	}

	virtual int NewChannelTask(wSocket* sock, wTask** ptr) {
		HNET_NEW(ExampleChannelTask(sock, mMaster), *ptr);
	    if (!*ptr) {
	    	HNET_ERROR(soft::GetLogPath(), "%s : %s", "ExampleServer::NewChannelTask new() failed", "");
	    	return -1;
	    }
	    return 0;
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

	// 版本输出 && 守护进程创建
	bool vn, dn;
	if (config->GetConf("version", &vn) && vn) {
		std::cout << soft::SetSoftName("example server -") << soft::GetSoftVer() << std::endl;
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

	// 创建服务器对象
	ExampleServer* server;
	HNET_NEW(ExampleServer(config), server);
	if (!server) {
		std::cout << "server new failed" << std::endl;
		HNET_DELETE(config);
		return -1;
	}

	// 创建master对象
	int ret = 0;
	wMaster* master;
	HNET_NEW(wMaster("EXAMPLE", server), master);
	if (master) {
	    std::string signal;
	    if (config->GetConf("signal", &signal) && signal.size() > 0) {	// 接受命令信号
	    	ret = master->SignalProcess(signal);
	    } else {
	    	// 准备服务器
	    	ret = master->PrepareStart();
			if (ret == 0) {
				ret = master->MasterStart();	// Master-Worker方式开启服务器
				if (ret == -1) {
					std::cout << "MasterStart failed" << std::endl;
				}
			} else {
				std::cout << "PrepareStart failed" << std::endl;
			}
	    }
	} else {
		std::cout << "master new failed" << std::endl;
	}

	HNET_DELETE(config);
	HNET_DELETE(server);
	HNET_DELETE(master);
	return ret;
}
