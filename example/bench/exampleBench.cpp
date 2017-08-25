
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <vector>
#include "wCore.h"
#include "wConfig.h"
#include "wSingleClient.h"
#include "exampleCmd.h"

#ifdef _USE_PROTOBUF_
#include "example.pb.h"
#endif

using namespace hnet;

pid_t SpawnProcess(int i, int request);
void Handle(int i, int request);
int exampleEchoWR();

static std::string hnet_host = "";
static uint16_t hnet_port = 0;

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

	// 版本输出
	bool version;
	if (config->GetConf("version", &version) && version) {
		std::cout << soft::SetSoftName("example client -") << soft::GetSoftVer() << std::endl;
		HNET_DELETE(config);
		return -1;
	}

	// 命令行-h、-p解析
    if (!config->GetConf("host", &hnet_host) || !config->GetConf("port", &hnet_port)) {
    	std::cout << "host or port error" << std::endl;
    	HNET_DELETE(config);
    	return -1;
    }

	// 开始微妙时间
	int64_t start_usec = misc::GetTimeofday();

	const int worker = 10;
	const int request = 5000;

	// 创建进程
	std::vector<pid_t> process(worker);
	for (int i = 0; i < worker; i++) {
		pid_t pid = SpawnProcess(i, request);
		if (pid > 0) {
			std::cout << "fork children:" << i << "|" << pid << std::endl;
			process[i] = pid;
		}
	}

	int error = 0;

	// 回收进程
	int status;
	while (!process.empty()) {
		pid_t pid = wait(&status);

		// 是否有错误请求
		if (WIFEXITED(status) != 0) {
			if (WEXITSTATUS(status) > 0) {
				error += WEXITSTATUS(status);
			}
		}

		std::vector<pid_t>::iterator it = std::find(process.begin(), process.end(), pid);
		if (it != process.end()) {
			std::cout << "recycle children:" << pid << std::endl;
			process.erase(it);
		}
	}

	int64_t total_usec = (misc::GetTimeofday() - start_usec)/1000000;

	std::cout << "[error]	:	" << error << std::endl;
	std::cout << "[success]	:	" << request*worker - error << std::endl;
	std::cout << "[second]	:	" << total_usec << "s" << std::endl;
	std::cout << "[qps]		:	" << request*worker/total_usec << "req/s" << std::endl;
	return 0;
}

pid_t SpawnProcess(int i, int request) {
	pid_t pid = fork();

	switch (pid) {
	case -1:
		exit(0);
		break;

	case 0:
		Handle(i, request);
		break;
	}
	return pid;
}

void Handle(int i, int request) {
	int64_t start_usec = misc::GetTimeofday();

	int num = 0;
	for (int i = 0; i < request; i++) {
		if (exampleEchoWR() != 0) {
			num++;
		}
	}

	int64_t total_usec = misc::GetTimeofday() - start_usec;
	std::cout << getpid() << "|" << "[error]	: " << num << std::endl;
	std::cout << getpid() << "|" << "[second]	: " << total_usec << "us" << std::endl;

	exit(num);
}

int exampleEchoWR() {
    // 创建客户端
	wSingleClient *client;
	HNET_NEW(wSingleClient, client);
    if (!client) {
    	std::cout << "client new failed" << std::endl;
        return -1;
    }
    
    // 连接服务器
    int ret = client->Connect(hnet_host, hnet_port);
    if (ret == -1) {
    	std::cout << "client connect failed" << std::endl;
    	HNET_DELETE(client);
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
    	HNET_DELETE(client);
		return -1;
	}

	// 同步接受服务器返回
#ifdef _USE_PROTOBUF_
	example::ExampleEchoRes res;
	ret = client->SyncRecv(&res, &size);

	if (ret == -1) {
		std::cout << "client receive failed" << s.ToString() << std::endl;
    	HNET_DELETE(client);
		return -1;
	}
#else
	example::ExampleResEcho_t res;
	ret = client->SyncRecv(reinterpret_cast<char*>(&res), &size, sizeof(res));

	if (ret == -1) {
		std::cout << "client receive failed" << std::endl;
    	HNET_DELETE(client);
		return -1;
	}
#endif
	
	HNET_DELETE(client);
	return 0;
}