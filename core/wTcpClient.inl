
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

template <typename T>
wTcpClient<T>::wTcpClient(int iType, string sClientName)
{
	mType = iType;
	mClientName = sClientName;
	mLastTicker = GetTickCount();
	mCheckTimer = wTimer(KEEPALIVE_TIME);
	mReconnectTimer = wTimer(KEEPALIVE_TIME);
}

template <typename T>
wTcpClient<T>::~wTcpClient()
{
	SAFE_DELETE(mTcpTask);
}

template <typename T>
int wTcpClient<T>::ReConnectToServer()
{
	if (mTcpTask != NULL && mTcpTask->Socket() != NULL)
	{
		if (mTcpTask->Socket()->FD() < 0)
		{
			return ConnectToServer(mTcpTask->Socket()->Host().c_str(), mTcpTask->Socket()->Port());
		}
		return 0;
	}
	return -1;
}

template <typename T>
int wTcpClient<T>::ConnectToServer(const char *vIPAddress, unsigned short vPort)
{
	if (vIPAddress == NULL || vPort == 0) return -1;
	
	wTcpSocket* pSocket = new wTcpSocket(SOCK_TYPE_CONNECT);
	if (pSocket->Open() == FD_UNKNOWN)
	{
		LOG_ERROR(ELOG_KEY, "[system] client create socket failed: %s", strerror(pSocket->Errno()));
		SAFE_DELETE(pSocket);
		return -1;
	}

	if (pSocket->Connect(vIPAddress, vPort) < 0)
	{
		LOG_ERROR(ELOG_KEY, "[system] client connect to server port(%d) failed: %s", vPort, strerror(pSocket->Errno()));
		SAFE_DELETE(pSocket);
		return -1;
	}
	pSocket->SockStatus() = SOCK_STATUS_CONNECTED;

	LOG_DEBUG(ELOG_KEY, "[system] client connect to %s:%d successfully", vIPAddress, vPort);
	
	//清除旧的socket
	if (mTcpTask != NULL && mTcpTask->Socket() != NULL)
	{
		SAFE_DELETE(mTcpTask);
	}

	mTcpTask = NewTcpTask(pSocket);
	if (NULL != mTcpTask)
	{
		if (mTcpTask->Verify() < 0 || mTcpTask->VerifyConn() < 0)
		{
			LOG_ERROR(ELOG_KEY, "[system] client connect illegal or verify timeout, close it");
			SAFE_DELETE(mTcpTask);
			return -1;
		}
		mTcpTask->Status() = TASK_RUNNING;
		if (mTcpTask->Socket()->SetNonBlock() < 0) 
		{
			LOG_ERROR(ELOG_KEY, "[system] client set non block failed, close it");
			SAFE_DELETE(mTcpTask);
			return -1;
		}
		return 0;
	}
	return -1;
}

template <typename T>
void wTcpClient<T>::PrepareStart()
{
	mStatus = CLIENT_RUNNING;
	PrepareRun();
}

template <typename T>
void wTcpClient<T>::Start(bool bDaemon)
{
	{
		Run();
	} while (bDaemon);
}

template <typename T>
void wTcpClient<T>::CheckTimer()
{
	unsigned long long iInterval = (unsigned long long)(GetTickCount() - mLastTicker);
	if (iInterval < 100) return;	//100ms

	mLastTicker += iInterval;
	if (mCheckTimer.CheckTimer(iInterval)) CheckTimeout();

	if (mReconnectTimer.CheckTimer(iInterval)) CheckReconnect();
}

template <typename T>
void wTcpClient<T>::CheckTimeout()
{
	if (!mIsCheckTimer || mTcpTask == NULL) return;

	unsigned long long iNowTime = GetTickCount();
	unsigned long long iIntervalTime;

	if (mTcpTask->Socket()->SockType() != SOCK_TYPE_CONNECT || mTcpTask->Socket()->SockStatus() != SOCK_STATUS_CONNECTED) return;
	
	//心跳检测
	iIntervalTime = iNowTime - mTcpTask->Socket()->SendTime();	//上一次发送时间间隔
	if (iIntervalTime >= KEEPALIVE_TIME)
	{
		if (mIsCheckTimer == true)	//使用业务层心跳机制
		{
			mTcpTask->Heartbeat();
			if (mTcpTask->HeartbeatOutTimes())
			{
				mTcpTask->Socket()->FD() = FD_UNKNOWN;
				LOG_DEBUG(ELOG_KEY, "[system] client disconnect server : out of heartbeat times");
			}
		}
		else	//使用keepalive保活机制（此逻辑一般不会被激活。也不必在业务层发送心跳，否则就失去了keepalive原始意义）
		{
			if (mTcpTask->Heartbeat() > 0)
			{
				mTcpTask->ClearbeatOutTimes();
			}
			if (mTcpTask->HeartbeatOutTimes())
			{
				mTcpTask->Socket()->FD() = FD_UNKNOWN;
				LOG_DEBUG(ELOG_KEY, "[system] client disconnect server : out of keepalive times");
			}
		}
	}
}

template <typename T>
void wTcpClient<T>::CheckReconnect()
{
	if (!mIsReconnect || mTcpTask != NULL) return;
	
	if (mTcpTask->Socket()->SockType() != SOCK_TYPE_CONNECT || mTcpTask->Socket()->SockStatus() != SOCK_STATUS_CONNECTED) return;
	
	if (mTcpTask->Socket() != NULL && mTcpTask->Socket()->FD() == FD_UNKNOWN)
	{
		if (ReConnectToServer() == 0)
		{
			LOG_INFO(ELOG_KEY, "[system] client reconnect success: ip(%s) port(%d)", mTcpTask->Socket()->Host().c_str(), mTcpTask->Socket()->Port());
		}
		else
		{
			LOG_ERROR(ELOG_KEY, "[system] client reconnect failed: ip(%s) port(%d)", mTcpTask->Socket()->Host().c_str(), mTcpTask->Socket()->Port());
		}
	}
}

template <typename T>
wTcpTask* wTcpClient<T>::NewTcpTask(wSocket *pSocket)
{
	T *pT = NULL;
	wTcpTask *pTcpTask = NULL;
	try
	{
		pT = new T(pSocket);
		pTcpTask = dynamic_cast<wTcpTask *>(pT);
	}
	catch(std::bad_cast& vBc)
	{
		LOG_ERROR(ELOG_KEY, "[system] bad_cast caught: : %s", vBc.what());
		SAFE_DELETE(pT);
		SAFE_DELETE(pTcpTask);
	}
	return pTcpTask;
}
