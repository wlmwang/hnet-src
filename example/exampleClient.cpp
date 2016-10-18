
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */
#include "wCore.h"
#include "wStatus.h"
#include "wSingleClient.h"
#include "exampleCmd.h"

int main(int argc, const char *argv[]) {
	wStatus s;

	wSingleClient *client;
	SAFE_NEW(wSingleClient, client);
    if (client == NULL) {
        return -1;
    }

    // 连接
    s = client->Connect("127.0.0.1", 10025);
    if (!s.Ok()) {
    	std::cout << "client connect failed" << s.ToString() << std::endl;
    	return -1;
    }

    //发送command
    struct ExampleReqEcho_t cmd;
    size_t l = strlen("hello hnet") + 1;
    memcpy(cmd.mCmd, "hello hnet", l);
    cmd.mLen = static_cast<uint8_t>(l);

	ssize_t size;
	s = client->SyncSend(reinterpret_cast<char *>(&cmd), sizeof(cmd), &size);
	if (!s.Ok()) {
		std::cout << "client send failed" << s.ToString() << std::endl;
		return -1;
	}

	s = client->SyncRecv(reinterpret_cast<char *>(&cmd), sizeof(cmd), &size);
	if (!s.Ok()) {
		std::cout << "client recv failed" << s.ToString() << std::endl;
		return -1;
	}

	std::cout << "server ret：" << cmd.mCmd << std::endl;
	return 0;
}
