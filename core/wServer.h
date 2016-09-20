
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SERVER_H_
#define _W_SERVER_H_

#include <algorithm>
#include <vector>
#include <sys/epoll.h>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wEnv;
class wTimer;
class wTask;
class wSocket;
class wMaster;
class wMutex;

// 服务基础类
class wServer : private wNoncopyable {
public:
	explicit wServer(string title);
	virtual ~wServer();
	
	wStatus Prepare(string ipaddr, uint16_t port, string protocol = "TCP");

	// single模式启动服务
	wStatus SingleStart(bool daemon = true);

	// master-worker多进程架构
	// PrepareMaster 需在master进程中调用
	// WorkerStart在worker进程提供服务
	wStatus WorkerStart(bool daemon = true);

	// 事件读写主调函数
	wStatus Recv();
	int Send(wTask *pTask, const char *pCmd, int iLen);
	void Broadcast(const char *pCmd, int iLen);
	

	void HandleSignal();
	void WorkerExit();
	
	// accept接受连接
	wStatus AcceptConn(wTask *task);

	// 新建客户端
	virtual wStatus NewTcpTask(wSocket* sock, wTask** ptr);
	virtual wStatus NewUnixTask(wSocket* sock, wTask** ptr);

	/**
	 * 服务主循环逻辑，继承可以定制服务
	 */
	virtual wStatus PrepareRun() {
		return mStatus = wStatus::IOError("wServer::PrepareRun, server will be exit", "method should be inherit");
	}

	virtual wStatus Run() {
		return mStatus = wStatus::IOError("wServer::Run, server will be exit", "method should be inherit");
	}
	
	virtual void CheckTimeout();
	static void CheckTimer(void* arg);

protected:
	friend class wMaster;

	wStatus InitEpoll();
	// Listen Socket(nonblock fd)
	wStatus AddListener(string ipaddr, uint16_t port, string protocol = "TCP");
	// 添加到epoll侦听事件队列
	wStatus Listener2Epoll();

	wStatus AddTask(wTask* task, int ev = EPOLLIN, int op = EPOLL_CTL_ADD);
	wStatus RemoveTask(wTask* task);
	wStatus CleanTask();

	wStatus AddToTaskPool(wTask *task);
	std::vector<wTask*>::iterator RemoveTaskPool(wTask *pTask);
	wStatus CleanTaskPool();

	wStatus mStatus;
	string mTitle;
	bool mExiting;
	wEnv *mEnv;

	vector<wSocket *> mListenSock;	// 多监听服务
	uint64_t mLatestTm;	// 服务器当前时间 微妙
	wTimer mHeartbeatTimer;	// 定时器
	bool mTimerSwitch;	// 心跳开关，默认关闭。强烈建议移动互联网环境下打开，而非依赖keepalive机制保活

	int32_t mEpollFD;
	uint64_t mTimeout;

	// task|pool
	wTask *mTask;
	vector<wTask*> mTaskPool;
	wMutex *mTaskPoolMutex;
};

}	// namespace hnet

#endif