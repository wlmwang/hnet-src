
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMaster.h"
#include "wMisc.h"
#include "wEnv.h"
#include "wSigSet.h"
#include "wSignal.h"
#include "wWorker.h"
#include "wChannelCmd.h"	//*channel*_t
#include "wConfig.h"
#include "wServer.h"

namespace hnet {

wMaster::wMaster(char* title, wServer* server, wConfig* config) : mPidPath(kPidPath), mSlot(kMaxPorcess), mWorkerNum(0),
mEnv(wEnv::Default()), mPid(getpid()), mTitle(title), mServer(server), mConfig(config) {
    mWorkerNum = mNcpu = sysconf(_SC_NPROCESSORS_ONLN);
}

wMaster::~wMaster() {
    for (uint32_t i = 0; i < mWorkerNum; ++i) {
		SAFE_DELETE(mWorkerPool[i]);
    }
}

wStatus wMaster::Prepare() {
    if (!PrepareRun().Ok()) {
    	return mStatus;
    }

    // 检测配置、服务实例
    if (mServer == NULL) {
    	return mStatus = wStatus::IOError("wMaster::PrepareStart failed", "mServer is null");
    } else if (mConfig == NULL || mConfig->mIPAddr == NULL || mConfig->mPort == 0) {
    	return mStatus = wStatus::IOError("wMaster::PrepareStart failed", "mConfig is null or ip|port is illegal");
    }

    // 进程标题
    mStatus = mConfig->mProcTitle->Setproctitle(kMasterTitle, mTitle.c_str());
    if (!mStatus.Ok()) {
    	return mStatus;
    }
    return mStatus = mServer->Prepare(mConfig->mIPAddr, mConfig->mPort, mConfig->mPtotocol);
}

wStatus wMaster::SingleStart() {
    if (!CreatePidFile().Ok()) {
		return mStatus;
    }
    
    // 恢复默认信号处理
    wSignal snl(SIG_DFL);
    snl.AddSigno(SIGINT);
    snl.AddSigno(SIGHUP);
    snl.AddSigno(SIGQUIT);
    snl.AddSigno(SIGTERM);
    snl.AddSigno(SIGCHLD);
    snl.AddSigno(SIGPIPE);
    snl.AddSigno(SIGTTIN);
    snl.AddSigno(SIGTTOU);
    
    if (!Run().Ok()) {
    	return mStatus;
    }
    return mStatus = mServer->SingleStart();
}

wStatus wMaster::MasterStart() {
	if (mWorkerNum > kMaxPorcess) {
		return mStatus = wStatus::IOError("wMaster::MasterStart, processes can be spawned", "worker number is overflow");
	} else if (!InitSignals().Ok()) {
		return mStatus;
	} else if (!CreatePidFile().Ok()) {
		return mStatus;
	}

	// 信号阻塞
	wSigSet sgt;
	sgt.AddSet(SIGCHLD);
	sgt.AddSet(SIGALRM);
	sgt.AddSet(SIGIO);
	sgt.AddSet(SIGINT);
	sgt.AddSet(SIGQUIT);
	sgt.AddSet(SIGTERM);
	sgt.AddSet(SIGHUP);	//RECONFIGURE
	sgt.AddSet(SIGUSR1);
	mStatus = sgt.Procmask();
	if (!mStatus.Ok()) {
		return mStatus;
	}
	
    // 启动worker进程
    if (!WorkerStart(mWorkerNum, kPorcessRespawn).Ok()) {
    	return mStatus;
    }

    // 监听信号
    while (true) {
		HandleSignal();
		Run();
	};
}


void wMaster::SingleExit() {
	DeletePidFile();
    //LOG_ERROR(ELOG_KEY, "[system] single process exit");
	exit(0);
}

void wMaster::MasterExit() {
	DeletePidFile();
    //LOG_ERROR(ELOG_KEY, "[system] master process exit");
	exit(0);
}

wStatus wMaster::HandleSignal() {
    int delay = 0;
	int sigio = 0;
	int live = 1;

	if (delay) {
		if (g_sigalrm) {
			sigio = 0;
			delay *= 2;
			g_sigalrm = 0;
		}

		struct itimerval itv;
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = delay / 1000;
		itv.it_value.tv_usec = (delay % 1000 ) * 1000;

		// 设置定时器，以系统真实时间来计算，送出SIGALRM信号
		if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
			mStatus = wStatus::IOError("wMaster::HandleSignal, setitimer() failed", strerror(errno));
		}
	}

	// 阻塞方式等待信号量
	wSigSet sigset;
	sigset.Suspend();
	
	// SIGCHLD有worker退出，重启
	if (g_reap) {
		g_reap = 0;

		ProcessExitStat();
		mStatus = ReapChildren(&live);
	}
	
	// worker都退出，且收到了SIGTERM信号或SIGINT信号(g_terminate ==1) 或SIGQUIT信号(g_quit == 1)，则master退出
	if (!live && (g_terminate || g_quit)) {
		//LOG_ERROR(ELOG_KEY, "[system] master process exiting");
		MasterExit();
	}
	
	// 收到SIGTERM信号或SIGINT信号(g_terminate ==1)，通知所有worker退出，并且等待worker退出
	// 平滑退出
	if (g_terminate) {
		// 设置延时
		if (delay == 0) {
			delay = 50;
		}
		if (sigio) {
			sigio--;
			return;
		}
		sigio = mWorkerNum;

		if (delay > 1000) {
			// 延时已到，给所有worker发送SIGKILL信号，强制杀死worker
			SignalWorker(SIGKILL);
		} else {
			// 给所有worker发送SIGTERM信号，通知worker退出
			SignalWorker(SIGTERM);
		}
		return;
	}
	
	// 强制退出
	if (g_quit) {
		SignalWorker(SIGQUIT);
		// 关闭所有监听socket
		// todo
		return;
	}
	
	// 收到SIGHUP信号（重启服务器）
	if (g_reconfigure) {
		//LOG_DEBUG(ELOG_KEY, "[system] reconfiguring master process");
		g_reconfigure = 0;
		
		// 重新初始化主进程配置
		ReloadMaster();
		// 重启worker
		WorkerStart(mWorkerNum, kProcessJustSpawn);
		
		/* allow new processes to start */
		//100ms
		usleep(100*1000);
		
		//LOG_DEBUG(ELOG_KEY, "[system] recycle old worker status");

		live = 1;
		// 关闭原来worker进程
		SignalWorker(SIGTERM);
	}
}

wStatus wMaster::ReapChildren(int* live) {
	*live = 0;
	for (uint32_t i = 0; i < mWorkerNum; i++) {
        //LOG_DEBUG(ELOG_KEY, "[system] reapchild pid(%d),exited(%d),exiting(%d),detached(%d),respawn(%d)", 
        //	mWorkerPool[i]->mPid, mWorkerPool[i]->mExited, mWorkerPool[i]->mExiting, mWorkerPool[i]->mDetached, mWorkerPool[i]->mRespawn);
        
        if (mWorkerPool[i] == NULL || mWorkerPool[i]->mPid == -1) {
            continue;
        }

        // 已退出
		if (mWorkerPool[i]->mExited) {
			// 非分离，就同步文件描述符
			if (!mWorkerPool[i]->mDetached) {
				// 关闭channel
				mWorkerPool[i]->mChannel->Close();

				struct ChannelReqClose_t ch;
				ch.mFD = -1;
				ch.mPid = mWorkerPool[i]->mPid;
				ch.mSlot = i;
				PassCloseChannel(&ch);
			}
			
			// 重启worker
			if (mWorkerPool[i]->mRespawn && !mWorkerPool[i]->mExiting && !g_terminate && !g_quit) {
				if (SpawnWorker(i) == -1) {
					//LOG_ERROR(ELOG_KEY, "[system] could not respawn %d", i);
					continue;
				}
				
				struct ChannelReqOpen_t ch;
				ch.mFD = mWorkerPool[mSlot]->mCh[0];
				ch.mPid = mWorkerPool[mSlot]->mPid;
				ch.mSlot = i;
				PassOpenChannel(&ch);
				
				*live = 1;
				continue;
			}
			
            if (i != (mWorkerNum - 1)) {
                mWorkerPool[i]->mPid = -1;
            } else {
            	//mWorkerNum--;
            }
		} else if (mWorkerPool[i]->mExiting || !mWorkerPool[i]->mDetached) {
			*live = 1;
		}
    }

    return mStatus = wStatus::Nothing();
}

wStatus WorkerStart(uint32_t n, int8_t type) {
	for (uint32_t i = 0; i < mWorkerNum; ++i) {
		if (!SpawnWorker(type).Ok()) {
			return mStatus;
		}
		struct ChannelReqOpen_t opench;
		opench.mSlot = mSlot;
        opench.mPid = mWorkerPool[mSlot]->mPid;
        opench.mFD = (*mWorkerPool[mSlot]->mChannel)[0];
        PassOpenChannel(&opench);
	}
}

void wMaster::SignalWorker(int signo) {
	struct ChannelReqQuit_t chopen;
	struct ChannelReqTerminate_t chclose;
	struct ChannelReqCmd_s* ch = NULL;

	int other = 0, size = 0;
	switch (signo) {
		case SIGQUIT:
		ch = &chopen;
		ch->mFD = -1;
		size = sizeof(struct ChannelReqQuit_t);
		break;
			
		case SIGTERM:
		ch = &chclose;
		ch->mFD = -1;
		size = sizeof(struct ChannelReqTerminate_t);
		break;

		default:
		other = 1;
	}

	char *ptr = NULL;
	if (!other) {
		ptr = new char[size + sizeof(int)];
		coding::EncodeFixed32(ptr, size);
	}
	
	for (uint32_t i = 0; i < mWorkerNum; i++) {

        if (mWorkerPool[i]->mDetached || mWorkerPool[i]->mPid == -1) {
        	continue;
        }
        
        if (mWorkerPool[i]->mJustSpawn) {
        	mWorkerPool[i]->mJustSpawn = 0;
        	continue;
        }

		if (mWorkerPool[i]->mExiting && signo == SIGQUIT) {
			continue;
		}
		
        if (!other) {
	        /* TODO: EAGAIN */
			memcpy(ptr + sizeof(int), reinterpret_cast<char *>(ch), size);
			if (mWorkerPool[i]->mChannel.SendBytes(ptr, size + sizeof(int)) >= 0) {
				if (signo == SIGQUIT || signo == SIGTERM) {
					mWorkerPool[i]->mExiting = 1;
				}
				continue;
			}
		}

        if (kill(mWorkerPool[i]->mPid, signo) == -1) {
			//LOG_ERROR(ELOG_KEY, "[system] kill(%d, %d) failed:%s", mWorkerPool[i]->mPid, signo, strerror(errno));
            
            if (errno == ESRCH) {
                mWorkerPool[i]->mExited = 1;
                mWorkerPool[i]->mExiting = 0;
				
                g_reap = 1;
            }
            continue;
        }
		
        if (signo != SIGUSR1) {
        	mWorkerPool[i]->mExiting = 1;
        }
    }
    
    if (!other) {
    	SAFE_DELETE_VEC(ptr);
    }
}

void wMaster::PassOpenChannel(struct ChannelReqOpen_t *ch) {
	size_t size = sizeof(struct ChannelReqOpen_t);
	char *ptr = new char[size + sizeof(int)];
	coding::EncodeFixed32(ptr, size);

	for (uint32_t i = 0; i < mWorkerNum; i++) {
		// 无需发送给自己
        if (i == mSlot || mWorkerPool[i]->mPid == -1) {
        	continue;
        }

        /* TODO: EAGAIN */
		memcpy(ptr + sizeof(int), reinterpret_cast<char*>(ch), size);
		mWorkerPool[i]->mChannel.SendBytes(ptr, size + sizeof(int));
    }
    SAFE_DELETE_VEC(ptr);
}

void wMaster::PassCloseChannel(struct ChannelReqClose_t *ch) {
	size_t size = sizeof(struct ChannelReqClose_t);
	char *ptr = new char[size + sizeof(int)];
	coding::EncodeFixed32(ptr, size);
    
	for (uint32_t i = 0; i < mWorkerNum; i++) {
		// 不发送已退出worker
		if (mWorkerPool[i]->mExited || mWorkerPool[i]->mPid == -1) {
			continue;
		}
        
        /* TODO: EAGAIN */
		memcpy(ptr + sizeof(int), reinterpret_cast<char*>(ch), size);
		mWorkerPool[i]->mChannel.SendBytes(ptr, size + sizeof(int));
    }
    SAFE_DELETE_VEC(pStart);
}

wStatus wMaster::NewWorker(uint32_t slot, wWorker** ptr) {
    SAFE_NEW(wWorker(mTitle, slot, this), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wMaster::NewWorker", "new failed");
    }
    return mStatus = wStatus::Nothing();
}

wStatus wMaster::SpawnWorker(int32_t type) {
	if (type >= 0) {
		mSlot = static_cast<uint32_t>(type);
	} else {
		for (uint32_t idx = 0; idx < mWorkerNum; ++idx) {
			if (mWorkerPool[idx] == NULL || mWorkerPool[idx]->mPid == -1) {
				break;
			}
		}
		mSlot = idx;
	}

	if (mSlot >= kMaxPorcess) {
		return mStatus::IOError("wMaster::SpawnWorker failed", "mSlot overflow");
	}

	// 新建进程表项
	if (!NewWorker(mSlot, &mWorkerPool[mSlot]).Ok()) {
		return mStatus;
	}
	wWorker *worker = mWorkerPool[mSlot];
	
	// 打开进程间IPC通信channel
	mStatus = worker->mChannel->Open();
	if (!mStatus.Ok()) {
		return mStatus;
	}

	// 设置第一个描述符的异步IO通知机制
	// FIOASYNC现已被O_ASYNC标志位取代
	// todo
	u_long on = 1;
    if (ioctl((*worker->mChannel)[0], FIOASYNC, &on) == -1) {
    	SAFE_DELETE(mWorkerPool[mSlot]);
    	return mStatus = wStatus::IOError("wMaster::SpawnWorker, ioctl(FIOASYNC) failed", strerror(errno));
    }
    // 设置将要在文件描述符channel[0]上接收SIGIO 或 SIGURG事件信号的进程标识
    if (fcntl((*worker->mChannel)[0], F_SETOWN, mPid) == -1) {
    	SAFE_DELETE(mWorkerPool[mSlot]);
    	return mStatus = wStatus::IOError("wMaster::SpawnWorker, fcntl(F_SETOWN) failed", strerror(errno));
    }

    pid_t pid = fork();
    switch (pid) {
	    case -1:
        SAFE_DELETE(mWorkerPool[mSlot]);
        return mStatus = wStatus::IOError("wMaster::SpawnWorker, fork() failed", strerror(errno));
			
	    case 0:
    	// worker进程
        worker->mPid = pid;
        mStatus = worker->PrepareStart();
        if (!mStatus.Ok()) {
        	return mStatus;
        }
        // 进入worker主循环
        worker->Start();
        _exit(0);
    }
    //LOG_INFO(ELOG_KEY, "[system] worker start %s [%d] %d", title, mSlot, pid);
    
    // 主进程master更新进程表
    worker->mSlot = mSlot;
    worker->mPid = pid;
	worker->mExited = 0;
	worker->mExiting = 0;
	
	if (type >= 0) {
		return mStatus = wStatus::Nothing();
	}

    switch (type) {
		case kPorcessNoRespawn:
		worker->mRespawn = 0;
		worker->mJustSpawn = 0;
		worker->mDetached = 0;
		break;

		case kPorcessRespawn:
		worker->mRespawn = 1;
		worker->mJustSpawn = 0;
		worker->mDetached = 0;
		break;
		
		case kPorcessJustSpawn:
		worker->mRespawn = 0;
		worker->mJustSpawn = 1;
		worker->mDetached = 0;    	

		case kPorcessJustRespawn:
		worker->mRespawn = 1;
		worker->mJustSpawn = 1;
		worker->mDetached = 0;

		case kPorcessDetached:
		worker->mRespawn = 0;
		worker->mJustSpawn = 0;
		worker->mDetached = 1;
    }
	
    return mStatus = wStatus::Nothing();
}

wStatus wMaster::CreatePidFile() {
	string pidstr = logging::NumberToString(mPid);
	return mStatus = WriteStringToFile(mEnv, pidstr, mPidPath);
}

wStatus wMaster::DeletePidFile() {
	return mStatus = mEnv->DeleteFile(mPidPath);
}

wStatus wMaster::InitSignals() {
	wSignal::Signal_t *sig;
	wSignal signal;
	for (sig = g_signals; sig->mSigno != 0; ++sig) {
		mStatus = signal.AddHandler(sig);
		if (!mStatus.Ok()) {
			return mStatus;
		}
	}
}

void wMaster::ProcessExitStat() {    
	const char *process = "unknown process";
	int one = 0;
	int status;
    while (true) {
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) {
            return;
        }
		
        if (pid == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == ECHILD && one) {
                return;
            }
            if (errno == ECHILD) {
				//LOG_ERROR(ELOG_KEY, "[system] waitpid() failed:%s", strerror(errno));
                return;
            }
			
			//LOG_ERROR(ELOG_KEY, "[system] waitpid() failed:%s", strerror(errno));
            return;
        }
		
        one = 1;
		int32_t i;
		for (i = 0; i < mWorkerNum; ++i) {
			if (mWorkerPool[i]->mPid == pid) {
                mWorkerPool[i]->mStat = status;
                mWorkerPool[i]->mExited = 1;	//退出
                process = mWorkerPool[i]->mTitle.c_str();
                break;
			}
		}
		
        if (WTERMSIG(status)) {
			//LOG_ERROR(ELOG_KEY, "[system] %s %d exited on signal %d%s", process, pid, WTERMSIG(status), WCOREDUMP(status) ? " (core dumped)" : "");
        } else {
			//LOG_ERROR(ELOG_KEY, "[system] %s %d exited with code %d", process, pid, WTERMSIG(status));
        }
		
		// 退出后不重启
        if (WEXITSTATUS(status) == 2 && mWorkerPool[i]->mRespawn) {
			//LOG_ERROR(ELOG_KEY, "[system] %s %d exited with fatal code %d and cannot be respawned", process, pid, WTERMSIG(status));
            mWorkerPool[i]->mRespawn = 0;
        }
    }
}

}	// namespace hnet
