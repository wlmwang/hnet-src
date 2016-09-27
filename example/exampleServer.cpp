
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
    ssize_t mLen;
    char* mCmd[512];
    ExampleReqEcho_t() : ExampleReqCmd_s(EXAMPLE_REQ_ECHO) { }
};

class ExampleTask : public wTcpTask {
public:
	ExampleTask(wSocket *socket, int32_t type) : wTcpTask(socket, type) {
		RegisterFunc(CMD_EXAMPLE_REQ, EXAMPLE_REQ_ECHO, &ExampleTask::ExampleEcho);
	}
    virtual const char* Name() const {
		return "ExampleTask";
    }

	DEC_FUNC(ExampleEcho);
};

int ExampleTask::ExampleEcho(char buf[], int len) {
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
	} else if (!config->GetOption(argc, argv).Ok()) {
		return -1;
	}

	if (config->mShowVer) {
		std::cout << kSoftwareName << kSoftwareVer << std::endl;
		return -1;
	} else if (config->mDaemon == 1) {
		if (!misc::InitDaemon().Ok(config->mLockPath)) {
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
		if (master->PrepareStart().Ok()) {
			master->MasterStart();
		} else {
			return -1;
		}
	}

	LOG_SHUTDOWN_ALL();
	return 0;
}
