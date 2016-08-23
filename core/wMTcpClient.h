
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MTCP_CLIENT_H_
#define _W_MTCP_CLIENT_H_

#include <algorithm>
#include <map>
#include <vector>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "wCore.h"
#include "wAssert.h"
#include "wTimer.h"
#include "wMisc.h"
#include "wLog.h"
#include "wTcpClient.h"
#include "wThread.h"

template <typename TASK>
class wMTcpClient : public wThread
{
	public:
		wMTcpClient();
		virtual ~wMTcpClient();
		
		/**
		 *  连接服务器
		 */
		bool GenerateClient(int iType, string sClientName, char *vIPAddress, unsigned short vPort);
		bool RemoveTcpClientPool(int iType, wTcpClient<TASK> *pTcpClient = NULL);
		void CleanTcpClientPool();
		int ResetTcpClientCount();
		
		/**
		 * epoll|socket相关
		 */
		int InitEpoll();
		void CleanEpoll();
		int AddToEpoll(wTcpClient<TASK> *pTcpClient, int iEvent = EPOLLIN | EPOLLERR | EPOLLHUP);
        int RemoveEpoll(wTcpClient<TASK> *pTcpClient);
		
		void PrepareStart();
		void Start(bool bDaemon = true);
		
		/**
		 * 线程入口函数
		 */
		virtual int Run();
		virtual int PrepareRun()  { return 0;}
		
		void CheckTimer();
		virtual void CheckTimeout();
		
		void Recv();
		void Send();
		
		/**
		 * 获取客户端连接
		 */
		vector<wTcpClient<TASK>*> TcpClients(int iType);
		wTcpClient<TASK>* OneTcpClient(int iType);
		
	protected:
		wTcpClient<TASK>* CreateClient(int iType, string sClientName, char *vIPAddress, unsigned short vPort);
		bool AddTcpClientPool(int iType, wTcpClient<TASK> *pTcpClient);

	protected:
		int mErr;
		int mEpollFD {FD_UNKNOWN};
		int mTimeout {10};
		
		//epoll_event
		struct epoll_event mEpollEvent;
		vector<struct epoll_event> mEpollEventPool; //epoll_event已发生事件池（epoll_wait）
		int mTcpClientCount {0};
		
		//定时记录器
		unsigned long long mLastTicker {0};	//服务器当前时间
        wTimer mCheckTimer;

		std::map<int, vector<wTcpClient<TASK>*> > mTcpClientPool;	//每种类型客户端，可挂载多个连接
};

#include "wMTcpClient.inl"

#endif
