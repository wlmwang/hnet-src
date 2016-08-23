
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

template <typename TASK>
wMTcpClient<TASK>::wMTcpClient()
{
	mLastTicker = GetTickCount();
	mCheckTimer = wTimer(KEEPALIVE_TIME);
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
	mEpollEventPool.reserve(LISTEN_BACKLOG);
}

template <typename TASK>
wMTcpClient<TASK>::~wMTcpClient()
{
	CleanEpoll();
	CleanTcpClientPool();
}

template <typename TASK>
void wMTcpClient<TASK>::CleanEpoll()
{
	if (mEpollFD != FD_UNKNOWN)
	{
		close(mEpollFD);
		mEpollFD = FD_UNKNOWN;
	}
	memset((void *)&mEpollEvent, 0, sizeof(mEpollEvent));
	mEpollEventPool.clear();
}

template <typename TASK>
void wMTcpClient<TASK>::CleanTcpClientPool()
{
    typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.begin();
	for (; mt != mTcpClientPool.end(); mt++)
	{
		vector<wTcpClient<TASK>* > vTcpClient = mt->second;
        typename vector<wTcpClient<TASK>*>::iterator vIt = vTcpClient.begin();
		for (; vIt != vTcpClient.end() ; vIt++)
		{
			SAFE_DELETE(*vIt);
		}
	}
	mTcpClientPool.clear();
	mTcpClientCount = 0;
}

template <typename TASK>
void wMTcpClient<TASK>::PrepareStart()
{
	if (InitEpoll() < 0) exit(0);
}

template <typename TASK>
void wMTcpClient<TASK>::Start(bool bDaemon)
{
	//进入服务主服务
	do {
		Recv();
		CheckTimer();
	} while (bDaemon);
}

template <typename TASK>
int wMTcpClient<TASK>::Run()
{
	Start(true);	//线程任务
	return 0;
}

template <typename TASK>
int wMTcpClient<TASK>::InitEpoll()
{
	if ((mEpollFD = epoll_create(LISTEN_BACKLOG)) < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] epoll_create failed:%s", strerror(mErr));
		return -1;
	}
	return mEpollFD;
}

template <typename TASK>
bool wMTcpClient<TASK>::GenerateClient(int iType, string sClientName, char *vIPAddress, unsigned short vPort)
{
	wTcpClient<TASK>* pTcpClient = CreateClient(iType, sClientName, vIPAddress , vPort);
	if (pTcpClient != NULL)
	{
		if (AddToEpoll(pTcpClient) >= 0)
		{
			return AddTcpClientPool(iType, pTcpClient);
		}
	}
	return false;
}

template <typename TASK>
wTcpClient<TASK>* wMTcpClient<TASK>::CreateClient(int iType, string sClientName, char *vIPAddress, unsigned short vPort)
{
	wTcpClient<TASK>* pTcpClient = new wTcpClient<TASK>(iType, sClientName);
	if (pTcpClient->ConnectToServer(vIPAddress, vPort) >= 0)
	{
		pTcpClient->PrepareStart();	//准备启动
		return pTcpClient;
	}
	SAFE_DELETE(pTcpClient);
	LOG_ERROR(ELOG_KEY, "[system] CreateClient connect to (%s)server faild!", sClientName.c_str());
	return NULL;
}

template <typename TASK>
bool wMTcpClient<TASK>::AddTcpClientPool(int iType, wTcpClient<TASK> *pTcpClient)
{
	W_ASSERT(pTcpClient != NULL, return false);

	vector<wTcpClient<TASK>*> vTcpClient;
    typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.find(iType);
	if (mt != mTcpClientPool.end())
	{
		vTcpClient = mt->second;
		mTcpClientPool.erase(mt);
	}
	vTcpClient.push_back(pTcpClient);
	mTcpClientPool.insert(pair<int, vector<wTcpClient<TASK>*> >(iType, vTcpClient));
	
	//客户端总数量
	ResetTcpClientCount();
	return true;
}

template <typename TASK>
int wMTcpClient<TASK>::ResetTcpClientCount()
{
	mTcpClientCount = 0;
    typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.begin();
	for (; mt != mTcpClientPool.end(); mt++)
	{
		vector<wTcpClient<TASK>* > vTcpClient = mt->second;
        //int iType = mt->first;
        typename vector<wTcpClient<TASK>*>::iterator vIt = vTcpClient.begin();
		for (; vIt != vTcpClient.end() ; vIt++)
		{
			mTcpClientCount++;
		}
	}
	
	//重置容量
	if (mTcpClientCount > (int)mEpollEventPool.capacity())
	{
		mEpollEventPool.reserve(mTcpClientCount * 2);
	}
	return mTcpClientCount;
}

template <typename TASK>
int wMTcpClient<TASK>::AddToEpoll(wTcpClient<TASK> *pTcpClient, int iEvent)
{
	int iSocketFD = pTcpClient->TcpTask()->Socket()->FD();
	mEpollEvent.events = iEvent;	//客户端事件
	mEpollEvent.data.fd = iSocketFD;
	mEpollEvent.data.ptr = pTcpClient;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, iSocketFD, &mEpollEvent) < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] client fd(%d) add into epoll failed: %s", iSocketFD, strerror(mErr));
		return -1;
	}
	return 0;
}

//心跳（包含重连）
template <typename TASK>
void wMTcpClient<TASK>::CheckTimer()
{
	unsigned long long iInterval = (unsigned long long)(GetTickCount() - mLastTicker);
	if (iInterval < 100) 	//100ms
	{
		return;
	}

	mLastTicker += iInterval;
	if (mCheckTimer.CheckTimer(iInterval))
	{
		CheckTimeout();
	}
}

template <typename TASK>
void wMTcpClient<TASK>::CheckTimeout()
{
    typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.begin();
	for (; mt != mTcpClientPool.end(); mt++)
	{
		vector<wTcpClient<TASK>* > vTcpClient = mt->second;
        typename vector<wTcpClient<TASK>*>::iterator vIt = vTcpClient.begin();
		for (; vIt != vTcpClient.end() ; vIt++)
		{
			if ((*vIt)->IsRunning())
			{
				(*vIt)->CheckTimer();
			}
			else
			{
				if (RemoveEpoll(*vIt) >= 0)
				{
					RemoveTcpClientPool(mt->first, *vIt);
					vIt--;
				}
			}
		}
	}
}

template <typename TASK>
void wMTcpClient<TASK>::Recv()
{
	int iRet = epoll_wait(mEpollFD, &mEpollEventPool[0], mTcpClientCount, mTimeout);
	if (iRet < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] client epoll_wait failed: %s", strerror(mErr));
		return;
	}
	
	wTcpClient<TASK> *pClient = NULL;
	wTcpTask *pTask = NULL;

	int iFD = FD_UNKNOWN;
	int iLenOrErr;
	for (int i = 0 ; i < iRet ; i++)
	{
		pClient = (wTcpClient<TASK>*) mEpollEventPool[i].data.ptr;
		pTask = pClient->TcpTask();
		iFD = pTask->Socket()->FD();

		if (iFD == FD_UNKNOWN)	//不删除此task，让其重连
		{
			continue;
		}
		if (!pClient->IsRunning() || !pTask->IsRunning())	//多数是超时设置
		{
			LOG_ERROR(ELOG_KEY, "[system] client task status is quit, fd(%d), close it", iFD);
			pClient->Status() = CLIENT_QUIT;
			continue;
		}
		if (mEpollEventPool[i].events & (EPOLLERR | EPOLLPRI))
		{
			mErr = errno;
			LOG_ERROR(ELOG_KEY, "[system] client epoll event recv error from fd(%d): %s, close it", iFD, strerror(mErr));
			pClient->Status() = CLIENT_QUIT;
			continue;
		}
		//client start
		pClient->Start(false);

		if (pTask->Socket()->SockType() == SOCK_TYPE_CONNECT && pTask->Socket()->SockStatus() == SOCK_STATUS_CONNECTED)	//connect event: read|write
		{
			if (mEpollEventPool[i].events & EPOLLIN)
			{
				//套接口准备好了读取操作
				if ((iLenOrErr = pTask->TaskRecv()) < 0)
				{
					if (iLenOrErr == ERR_CLOSED)
					{
						LOG_DEBUG(ELOG_KEY, "[system] client tcp socket closed by server");
					}
					else if (iLenOrErr == ERR_MSGLEN)
					{
						LOG_ERROR(ELOG_KEY, "[system] client recv message invalid len");
					}
					else
					{
						LOG_ERROR(ELOG_KEY, "[system] EPOLLIN(read) failed or server-socket closed: %s", strerror(pTask->Socket()->Errno()));
					}
					pTask->Socket()->FD() = FD_UNKNOWN;		//给重连做准备
				}
			}
			else if (mEpollEventPool[i].events & EPOLLOUT)
			{
				//套接口准备好了写入操作
				if (pTask->TaskSend() < 0)
				{
					LOG_ERROR(ELOG_KEY, "[system] EPOLLOUT(write) failed: %s", strerror(pTask->Socket()->Errno()));
					pTask->Socket()->FD() = FD_UNKNOWN;		//给重连做准备
				}
			}
		}
	}
}

template <typename TASK>
int wMTcpClient<TASK>::RemoveEpoll(wTcpClient<TASK> *pTcpClient)
{
	int iSocketFD = pTcpClient->TcpTask()->Socket()->FD();
	mEpollEvent.data.fd = iSocketFD;
	if (epoll_ctl(mEpollFD, EPOLL_CTL_DEL, iSocketFD, &mEpollEvent) < 0)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] client epoll remove socket fd(%d) error : %s", iSocketFD, strerror(mErr));
		return -1;
	}
	return 0;
}

template <typename TASK>
bool wMTcpClient<TASK>::RemoveTcpClientPool(int iType, wTcpClient<TASK> *pTcpClient)
{
	typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.find(iType);
	if (mt != mTcpClientPool.end())
	{
		vector<wTcpClient<TASK>*> vTcpClient = mt->second;
		if (pTcpClient == NULL)
		{
			typename vector<wTcpClient<TASK>*>::iterator it = vTcpClient.begin();
			for (; it != vTcpClient.end(); it++)
			{
				SAFE_DELETE(*it);
			}
			mTcpClientPool.erase(mt);
			return true;
		}
		else
		{
			typename vector<wTcpClient<TASK>*>::iterator it = find(vTcpClient.begin(), vTcpClient.end(), pTcpClient);
			if (it != vTcpClient.end())
			{
				vTcpClient.erase(it);
				SAFE_DELETE(*it);
				mTcpClientPool.erase(mt);
				mTcpClientPool.insert(pair<int, vector<wTcpClient<TASK>*> >(iType, vTcpClient));
				return true;
			}
			return false;
		}
	}
	return false;
}

template <typename TASK>
vector<wTcpClient<TASK>*> wMTcpClient<TASK>::TcpClients(int iType)
{
	vector<wTcpClient<TASK>*> vTcpClient;
    typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.find(iType);
	if (mt != mTcpClientPool.end())
	{
		vTcpClient = mt->second;
	}
	return vTcpClient;
}

template <typename TASK>
wTcpClient<TASK>* wMTcpClient<TASK>::OneTcpClient(int iType)
{
	vector<wTcpClient<TASK>*> vTcpClient;
    typename map<int, vector<wTcpClient<TASK>*> >::iterator mt = mTcpClientPool.find(iType);
	if (mt != mTcpClientPool.end())
	{
		vTcpClient = mt->second;
		return vTcpClient[0];	//第一个连接
	}
	return NULL;
}
