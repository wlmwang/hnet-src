
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wMisc.h"
#include "wLog.h"
#include "wCommand.h"
#include "wTcpTask.h"
#include "wConfig.h"
#include "wServer.h"
#include "wMaster.h"

using namespace hnet;

const uint8_t CMD_EXAMPLE_REQ = 100;
struct ExampleReqCmd_s : public wCommand {
    ExampleReqCmd_s(uint8_t para) : wCommand(CMD_EXAMPLE_REQ, para) { }
};
const uint8_t EXAMPLE_REQ_ECHO = 0;
struct ExampleReqEcho_t : public ExampleReqCmd_s {
    int8_t mLen;
    char mCmd[512];
    ExampleReqEcho_t() : ExampleReqCmd_s(EXAMPLE_REQ_ECHO) { }
};

class ExampleTask : public wTcpTask {
public:
	ExampleTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		REG_DISP(mDispatch, Name(), CMD_EXAMPLE_REQ, EXAMPLE_REQ_ECHO, &ExampleTask::ExampleEcho);
	}
    virtual const char* Name() const {
		return "ExampleTask";
    }

	DEC_FUNC(ExampleEcho);
};

int ExampleTask::ExampleEcho(char buf[], uint32_t len) {
	struct ExampleReqEcho_t* cmd = reinterpret_cast<struct ExampleReqEcho_t*>(buf);
	ssize_t size;
	SyncSend(cmd->mCmd, cmd->mLen, &size);
	return 0;
}

class ExampleServer : public wServer {
public:
	virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr) {
	    SAFE_NEW(ExampleTask(sock, Shard(sock)), *ptr);
	    if (*ptr == NULL) {
			return wStatus::IOError("ExampleServer::NewTcpTask", "ExampleTask new failed");
	    }
	    return mStatus;
	}
};

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
	bool daemon;
	if (config->GetConf("daemon", &daemon) && daemon == true) {
		std::string lock_path;
		config->GetConf("lock_path", &lock_path);
		if (!misc::InitDaemon(lock_path).Ok()) {
			std::cout << "create daemon failed" << std::endl;
			return -1;
		}
	}

	ExampleServer* server;
	SAFE_NEW(ExampleServer, server);
	if (server == NULL) {
		return -1;
	}

	wMaster* master;
	SAFE_NEW(wMaster("EXAMPLE", server, config), master);
	if (master != NULL) {
		s = master->PrepareStart();
		if (s.Ok()) {
			wStatus s = master->MasterStart();
			std::cout << "master start:" << s.ToString() << std::endl;
		} else {
			std::cout << "master prepare start:" << s.ToString() << std::endl;
			return -1;
		}
	}

	LOG_SHUTDOWN_ALL();
	return 0;
}
