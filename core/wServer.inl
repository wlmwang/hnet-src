
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

template <typename T>
wServer<T>::wServer(string sName)
{
	mServerName = sName;
	mCheckTimer = wTimer(KEEPALIVE_TIME);
	mEpollEventPool.reserve(LISTEN_BACKLOG);
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
	mLastTicker = GetTickCount();
}

template <typename T>
wServer<T>::~wServer()
{
	CleanEpoll();
	CleanTaskPool();
}

template <typename T>
void wServer<T>::PrepareSingle(string sIpAddr, unsigned int nPort, string sProtocol)
{
	LOG_INFO(ELOG_KEY, "[system] single server prepare on ip(%s) port(%d) protocol(%s)", sIpAddr.c_str(), nPort, sProtocol.c_str());
	
	//添加服务器
	if (AddListener(sIpAddr, nPort, sProtocol) < 0 || mListenSock.size() <= 0) exit(0);
	
	//初始化epoll
	if (InitEpoll() < 0) exit(0);
	
	//添加到侦听事件队列
	Listener2Epoll();
	
	//运行前工作
	PrepareRun();
}

template <typename T>
void wServer<T>::SingleStart(bool bDaemon)
{
	LOG_INFO(ELOG_KEY, "[system] single server start succeed");
	mStatus = SERVER_RUNNING;

	//进入服务主循环
	do {
		Recv();
		Run();
		CheckTimer();
	} while(IsRunning() && bDaemon);
}

template <typename T>
void wServer<T>::PrepareMaster(string sIpAddr, unsigned int nPort, string sProtocol)
{
	LOG_INFO(ELOG_KEY, "[system] master server prepare on ip(%s) port(%d) protocol(%s)", sIpAddr.c_str(), nPort, sProtocol.c_str());
	
	//添加服务器
	if (AddListener(sIpAddr, nPort, sProtocol) < 0 || mListenSock.size() <= 0) exit(0);

	//运行前工作
	PrepareRun();
}

template <typename T>
void wServer<T>::WorkerStart(bool bDaemon)
{
	LOG_INFO(ELOG_KEY, "[system] worker server start succeed");
	mStatus = SERVER_RUNNING;
	
	//初始化epoll
	if (InitEpoll() < 0) exit(2);
	
	//添加到侦听事件队列
	Listener2Epoll();

	//进入服务主循环
	do {
		if (mExiting) WorkerExit();
		Recv();
		HandleSignal();
		Run();
		CheckTimer();
	} while (IsRunning() && bDaemon);
}

template <typename T>
void wServer<T>::HandleSignal()
{	
	if (g_terminate) WorkerExit();	//直接退出
	
	if (g_quit)		//平滑退出
	{
		g_quit = 0;

		LOG_ERROR(ELOG_KEY, "[system] gracefully shutting down");
		if (!mExiting)
		{
			mExiting = 1;
			for (vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++) (*it)->Close(); //关闭listen socket
		}
	}
}

template <typename T>
void wServer<T>::WorkerExit()
{
	if (mExiting) {/** 关闭连接池 */}

	LOG_ERROR(ELOG_KEY, "[system] worker process exiting");
	exit(0);
}

/**
 * epoll初始化
 */
template <typename T>
int wServer<T>::InitEpoll()
{
	if ((mEpollFD = epoll_create(LISTEN_BACKLOG)) < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] epoll_create failed:%s", strerror(mErr));
		return -1;
	}
	return mEpollFD;
}

template <typename T>
wTask* wServer<T>::NewTcpTask(wSocket *pSocket)
{
	return new wTcpTask(pSocket);
}

template <typename T>
wTask* wServer<T>::NewUnixTask(wSocket *pSocket)
{
	return new wUnixTask(pSocket);
}

template <typename T>
int wServer<T>::AddToEpoll(wTask* pTask, int iEvents, int iOp)
{
	W_ASSERT(pTask != NULL, return -1);

	mEpollEvent.events = iEvents | EPOLLERR | EPOLLHUP; //|EPOLLET
	mEpollEvent.data.fd = pTask->Socket()->FD();
	mEpollEvent.data.ptr = pTask;
	if (epoll_ctl(mEpollFD, iOp, pTask->Socket()->FD(), &mEpollEvent) < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] fd(%d) add into epoll failed: %s", pTask->Socket()->FD(), strerror(mErr));
		return -1;
	}
	LOG_DEBUG(ELOG_KEY, "[system] %s fd %d events read %d write %d", 
		iOp == EPOLL_CTL_MOD ? "mod":"add", pTask->Socket()->FD(), mEpollEvent.events & EPOLLIN, mEpollEvent.events & EPOLLOUT);
	return 0;
}

template <typename T>
int wServer<T>::AddToTaskPool(wTask* pTask)
{
	W_ASSERT(pTask != NULL, return -1);

	mTaskPool.push_back(pTask);
	mTaskCount = mTaskPool.size();	//epoll_event大小
	if (mTaskCount > (int)mEpollEventPool.capacity())
	{
		mEpollEventPool.reserve(mTaskCount * 2);
	}
	LOG_DEBUG(ELOG_KEY, "[system] fd(%d) add into task pool", pTask->Socket()->FD());
	return 0;
}

template <typename T>
void wServer<T>::Recv()
{
	int iRet = epoll_wait(mEpollFD, &mEpollEventPool[0], mTaskCount, mTimeout);
	if (iRet < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] epoll_wait failed: %s", strerror(mErr));
		return;
	}
	
	int iFD = FD_UNKNOWN;
	int iLenOrErr = 0;
	wTask *pTask = NULL;
	for (int i = 0 ; i < iRet ; i++)
	{
		pTask = (wTask *)mEpollEventPool[i].data.ptr;
		iFD = pTask->Socket()->FD();
		if (iFD == FD_UNKNOWN)
		{
			LOG_DEBUG(ELOG_KEY, "[system] socket FD is error, fd(%d), close it", iFD);
			if (RemoveEpoll(pTask) >= 0)
			{
				RemoveTaskPool(pTask);
			}
			continue;
		}
		if (!pTask->IsRunning())	//多数是超时设置
		{
			LOG_DEBUG(ELOG_KEY, "[system] task status is quit, fd(%d), close it", iFD);
			if (RemoveEpoll(pTask) >= 0)
			{
				RemoveTaskPool(pTask);
			}
			continue;
		}
		if (mEpollEventPool[i].events & (EPOLLERR | EPOLLPRI))	//出错(多数为sock已关闭)
		{
			mErr = errno;
			LOG_ERROR(ELOG_KEY, "[system] epoll event recv error from fd(%d), close it: %s", iFD, strerror(mErr));
			if (RemoveEpoll(pTask) >= 0)
			{
				RemoveTaskPool(pTask);
			}
			continue;
		}
		
		if (pTask->Socket()->SockType() == SOCK_TYPE_LISTEN && pTask->Socket()->SockStatus() == SOCK_STATUS_LISTENED)
		{
			if (mEpollEventPool[i].events & EPOLLIN)
			{
				AcceptConn(pTask);	//accept connect
			}
		}
		else if (pTask->Socket()->SockType() == SOCK_TYPE_CONNECT && pTask->Socket()->SockStatus() == SOCK_STATUS_CONNECTED)
		{
			if (mEpollEventPool[i].events & EPOLLIN)
			{
				//套接口准备好了读取操作
				if ((iLenOrErr = pTask->TaskRecv()) < 0)
				{
					if (iLenOrErr == ERR_CLOSED)
					{
						LOG_DEBUG(ELOG_KEY, "[system] socket closed by client");
					}
					else if (iLenOrErr == ERR_MSGLEN)
					{
						LOG_ERROR(ELOG_KEY, "[system] recv message invalid len");
					}
					else
					{
						LOG_ERROR(ELOG_KEY, "[system] EPOLLIN(read) failed or socket closed: %s", strerror(pTask->Socket()->Errno()));
					}
					if (RemoveEpoll(pTask) >= 0)
					{
						RemoveTaskPool(pTask);
					}
				}
			}
			else if (mEpollEventPool[i].events & EPOLLOUT)
			{
				//清除写事件
				if (pTask->WritableLen() <= 0)
				{
					AddToEpoll(pTask, EPOLLIN, EPOLL_CTL_MOD);
					continue;
				}
				//套接口准备好了写入操作
				if (pTask->TaskSend() < 0)	//写入失败，半连接，对端读关闭
				{
					LOG_ERROR(ELOG_KEY, "[system] EPOLLOUT(write) failed or socket closed: %s", strerror(pTask->Socket()->Errno()));
					if (RemoveEpoll(pTask) >= 0)
					{
						RemoveTaskPool(pTask);
					}
				}
			}
		}
	}
}

template <typename T>
int wServer<T>::Send(wTask *pTask, const char *pCmd, int iLen)
{
	W_ASSERT(pTask != NULL && pTask->IsRunning(), return -1);

	if (pTask->Socket()->SockType() == SOCK_TYPE_CONNECT && pTask->Socket()->SockStatus() == SOCK_STATUS_CONNECTED && (pTask->Socket()->SockFlag() == SOCK_FLAG_SEND || pTask->Socket()->SockFlag() == SOCK_FLAG_RVSD))
	{
		if (pTask->Send2Buf(pCmd, iLen) > 0)
		{
			return AddToEpoll(pTask, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD);
		}
	}
	return -1;
}

template <typename T>
void wServer<T>::Broadcast(const char *pCmd, int iLen)
{
	W_ASSERT(pCmd != NULL, return);

	if (mTaskPool.size() > 0)
	{
		for (vector<wTask*>::iterator iter = mTaskPool.begin(); iter != mTaskPool.end(); iter++)
		{
			if (*iter != NULL) Send(*iter, pCmd, iLen);
		}
	}
}

template <typename T>
int wServer<T>::AddListener(string sIpAddr, unsigned int nPort, string sProtocol)
{
	int idx = -1;
	wSocket *pSocket = NULL;
	if (sProtocol == "TCP")
	{
		pSocket = new wTcpSocket(SOCK_TYPE_LISTEN);
	}
	else if(sProtocol == "UNIX")
	{
		pSocket = new wUnixSocket(SOCK_TYPE_LISTEN);
	}

	if (pSocket != NULL)
	{
		if (pSocket->Open() == FD_UNKNOWN)
		{
			LOG_ERROR(ELOG_KEY, "[system] listen socket open failed:%s", strerror(pSocket->Errno()));
			SAFE_DELETE(pSocket);
			return -1;
		}
		if (pSocket->Listen(sIpAddr, nPort) < 0)
		{
			LOG_ERROR(ELOG_KEY, "[system] listen failed: %s", strerror(pSocket->Errno()));
			SAFE_DELETE(pSocket);
			return -1;
		}
		pSocket->SockStatus() = SOCK_STATUS_LISTENED;
		mListenSock.push_back(pSocket);
		idx = mListenSock.size();
	}
	return idx - 1;
}

template <typename T>
void wServer<T>::Listener2Epoll()
{
	//添加到侦听事件队列
	for (vector<wSocket *>::iterator it = mListenSock.begin(); it != mListenSock.end(); it++)
	{
		mTask = NULL;
		if ((*it)->SockProto() == SOCK_PROTO_UNIX)
		{
			mTask = NewUnixTask(*it);
		}
		else if((*it)->SockProto() == SOCK_PROTO_TCP)
		{
			mTask = NewTcpTask(*it);
		}
		if (NULL != mTask)
		{
			mTask->Status() = TASK_RUNNING;
			if (AddToEpoll(mTask) >= 0)
			{
				AddToTaskPool(mTask);
			}
			else
			{
				SAFE_DELETE(mTask);
				exit(2);
			}
		}
	}
}

template <typename T>
int wServer<T>::AcceptConn(wTask *pTask)
{
	mTask = NULL;
	int iNewFD = FD_UNKNOWN;
	if (pTask->Socket()->SockProto() == SOCK_PROTO_UNIX)
	{
		struct sockaddr_un stSockAddr;
		socklen_t iSockAddrSize = sizeof(stSockAddr);
		iNewFD = pTask->Socket()->Accept((struct sockaddr*)&stSockAddr, &iSockAddrSize);
		if (iNewFD <= 0)
		{
			if (iNewFD < 0) LOG_ERROR(ELOG_KEY, "[system] unix client connect failed:%s", strerror(pTask->Socket()->Errno()));
		    return iNewFD;
	    }
	    //unix socket
		wUnixSocket *pSocket = new wUnixSocket(SOCK_TYPE_CONNECT);
		pSocket->FD() = iNewFD;
		pSocket->Host() = stSockAddr.sun_path;
		pSocket->SockStatus() = SOCK_STATUS_CONNECTED;
		if (pSocket->SetNonBlock() < 0)
		{
			//SAFE_DELETE(pSocket);
			//return -1;
		}
		mTask = NewUnixTask(pSocket);
	}
	else if(pTask->Socket()->SockProto() == SOCK_PROTO_TCP)
	{
		struct sockaddr_in stSockAddr;
		socklen_t iSockAddrSize = sizeof(stSockAddr);	
		iNewFD = pTask->Socket()->Accept((struct sockaddr*)&stSockAddr, &iSockAddrSize);
		if (iNewFD <= 0)
		{
			if (iNewFD < 0) LOG_ERROR(ELOG_KEY, "[system] client connect failed:%s", strerror(pTask->Socket()->Errno()));
		    return iNewFD;
	    }
		//tcp socket
		wTcpSocket *pSocket = new wTcpSocket(SOCK_TYPE_CONNECT);
		pSocket->FD() = iNewFD;
		pSocket->Host() = inet_ntoa(stSockAddr.sin_addr);
		pSocket->Port() = stSockAddr.sin_port;
		pSocket->SockStatus() = SOCK_STATUS_CONNECTED;
		if (pSocket->SetNonBlock() < 0)
		{
			//SAFE_DELETE(pSocket);
			//return -1;
		}
		mTask = NewTcpTask(pSocket);
	}
	
	if (NULL != mTask)
	{
		if (mTask->VerifyConn() < 0 || mTask->Verify())
		{
			LOG_ERROR(ELOG_KEY, "[system] connect illegal or verify timeout: %d, close it", iNewFD);
			SAFE_DELETE(mTask);
			return -1;
		}
		
		mTask->Status() = TASK_RUNNING;
		if (AddToEpoll(mTask) >= 0)
		{
			AddToTaskPool(mTask);
		}
		else
		{
			SAFE_DELETE(mTask);
			return -1;
		}
		LOG_DEBUG(ELOG_KEY, "[system] client connect succeed: Host(%s)", mTask->Socket()->Host().c_str());
	}
	return iNewFD;
}

template <typename T>
void wServer<T>::CleanEpoll()
{
	if (mEpollFD != -1) close(mEpollFD);
	mEpollFD = -1;
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
	mEpollEventPool.clear();
}

template <typename T>
void wServer<T>::CleanTaskPool()
{
	if (mTaskPool.size() > 0)
	{
		vector<wTask*>::iterator it;
		for (it = mTaskPool.begin(); it != mTaskPool.end(); it++)
		{
			SAFE_DELETE(*it);
		}
	}
	mTaskPool.clear();
	mTaskCount = 0;
}

template <typename T>
int wServer<T>::RemoveEpoll(wTask* pTask)
{
	int iFD = pTask->Socket()->FD();
	mEpollEvent.data.fd = iFD;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, iFD, &mEpollEvent) < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] epoll remove socket fd(%d) error : %s", iFD, strerror(mErr));
		return -1;
	}
	return 0;
}

//返回下一个迭代器
template <typename T>
std::vector<wTask*>::iterator wServer<T>::RemoveTaskPool(wTask* pTask)
{
    std::vector<wTask*>::iterator it = std::find(mTaskPool.begin(), mTaskPool.end(), pTask);
    if (it != mTaskPool.end())
    {
    	SAFE_DELETE(*it);
        it = mTaskPool.erase(it);
    }
    mTaskCount = mTaskPool.size();
    return it;
}

template <typename T>
void wServer<T>::CheckTimer()
{
	unsigned long long iInterval = (unsigned long long)(GetTickCount() - mLastTicker);
	if (iInterval < 100) return;	//100ms

	mLastTicker += iInterval;
	if (mCheckTimer.CheckTimer(iInterval)) CheckTimeout();
}

template <typename T>
void wServer<T>::CheckTimeout()
{
	if (!mIsCheckTimer) return;

	unsigned long long iNowTime = GetTickCount();
	unsigned long long iIntervalTime;
	if (mTaskPool.size() > 0)
	{
		for (vector<wTask*>::iterator iter = mTaskPool.begin(); iter != mTaskPool.end(); iter++)
		{
			if ((*iter)->Socket()->SockType() == SOCK_TYPE_CONNECT && (*iter)->Socket()->SockStatus() == SOCK_STATUS_CONNECTED)
			{
				//心跳检测
				iIntervalTime = iNowTime - (*iter)->Socket()->SendTime();	//上一次发送时间间隔
				if (iIntervalTime >= KEEPALIVE_TIME)	//3s
				{
					bool bDelTask = false;
					if (mIsCheckTimer == true)	//使用业务层心跳机制
					{
						(*iter)->Heartbeat();
						if ((*iter)->HeartbeatOutTimes())
						{
							bDelTask = true;
						}
					}
					else	//使用keepalive保活机制（此逻辑一般不会被激活。也不必在业务层发送心跳，否则就失去了keepalive原始意义）
					{
						if ((*iter)->Heartbeat() > 0)
						{
							(*iter)->ClearbeatOutTimes();
						}
						if ((*iter)->HeartbeatOutTimes())
						{
							bDelTask = true;
						}
					}

					//关闭无用连接
					if (bDelTask)
					{
						LOG_ERROR(ELOG_KEY, "[system] client fd(%d) heartbeat pass limit times, close it", (*iter)->Socket()->FD());
						if (RemoveEpoll(*iter) >= 0)
						{
							iter = RemoveTaskPool(*iter);
							iter--;
						}
					}
				}
			}
		}
	}
}

/*
template <typename T>
int wServer<T>::AcceptMutexLock()
{
	return 0;
	if (mWorker != NULL && mWorker->mMutex)
	{
		if (mWorker->mMutexHeld == 1)
		{
			return 0;
		}
		if (mWorker->mMutex->TryLock() == 0)
		{
			mWorker->mMutexHeld = 1;
			return 0;
		}
		return -1;
	}
	return 0;
}

template <typename T>
int wServer<T>::AcceptMutexUnlock()
{
	return 0;
	if (mWorker != NULL && mWorker->mMutex)
	{
		if (mWorker->mMutexHeld == 0)
		{
			return 0;
		}
		if (mWorker->mMutex->Unlock() == 0)
		{
			mWorker->mMutexHeld = 0;
			return 0;
		}
		return -1;
	}
	return 0;
}
*/
