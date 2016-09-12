
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMaster.h"
#include "wMisc.h"
#include "wEnv.h"
#include "wLog.h"
#include "wSigSet.h"
#include "wSignal.h"
#include "wWorker.h"
#include "wChannelCmd.h"	//*channel*_t
//#include "wShm.h"
//#include "wShmtx.h"

namespace hnet {

wMaster::wMaster() : mEnv(wEnv::Default()), mPidPath(kPidPath), mWorkerNum(0), mSlot(kMaxPorcess), mDelay(500) {
    mPid = getpid();
    mNcpu = sysconf(_SC_NPROCESSORS_ONLN);
}

wMaster::~wMaster() {
    for (uint32_t i = 0; i < mWorkerNum; ++i) {
		SAFE_DELETE(mWorkerPool[i]);
    }
}

wStatus wMaster::NewWorker(const char* title, uint32_t slot, wWorker** ptr) {
    SAFE_NEW(wWorker(title, slot, this), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wMaster::NewWorker", "new failed");
    }
    return mStatus = wStatus::Nothing();
}

wStatus wMaster::PrepareStart() {
    mWorkerNum = mNcpu;
    return PrepareRun();
}

wStatus wMaster::SingleStart() {
    //mProcess = PROCESS_SINGLE;
    
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
    
    return Run();
}

wStatus wMaster::MasterStart() {
	//LOG_INFO(ELOG_KEY, "[system] master process start pid(%d)", getpid());

	//mProcess = PROCESS_MASTER;
	
	if (mWorkerNum > kMaxPorcess) {
		return mstatus = wStatus::IOError("wMaster::MasterStart, processes can be spawned", "worker number is overflow");
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

void wMaster::HandleSignal() {
	struct itimerval itv;
    int delay = 0;
	int sigio = 0;
	int iLive = 1;

	if (delay) {
		if (g_sigalrm) {
			sigio = 0;
			delay *= 2;
			g_sigalrm = 0;
		}
		
		//LOG_ERROR(ELOG_KEY, "[system] termination cycle: %d", delay);

		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = delay / 1000;
		itv.it_value.tv_usec = (delay % 1000 ) * 1000;

		// 设置定时器，以系统真实时间来计算，送出SIGALRM信号
		if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
			mErr = errno;
			LOG_ERROR(ELOG_KEY, "[system] setitimer() failed: %s", strerror(mErr));
		}
	}

	// 阻塞方式等待信号量
	wSigSet stSigset;
	stSigset.Suspend();
	
	// SIGCHLD有worker退出，重启
	if (g_reap) {
		g_reap = 0;

		LOG_ERROR(ELOG_KEY, "[system] reap children(worker process)");
		ProcessGetStatus();
		iLive = ReapChildren();
	}
	
	// worker都退出，且收到了SIGTERM信号或SIGINT信号(g_terminate ==1) 或SIGQUIT信号(g_quit == 1)，则master退出
	if (!iLive && (g_terminate || g_quit)) {
		LOG_ERROR(ELOG_KEY, "[system] master process exiting");
		MasterExit();
	}
	
	// 收到SIGTERM信号或SIGINT信号(g_terminate ==1)，通知所有worker退出，并且等待worker退出
	// 平滑退出
	if (g_terminate) {
		if (delay == 0) {
			delay = 50;     //设置延时
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
		
		ReloadMaster();	//重新初始化主进程配置
		WorkerStart(mWorkerNum, PROCESS_JUST_SPAWN);	//重启worker
		
		/* allow new processes to start */
		usleep(100*1000);	//100ms
		
		//LOG_DEBUG(ELOG_KEY, "[system] recycle old worker status");

		iLive = 1;
		SignalWorker(SIGTERM);	//关闭原来worker进程
	}
}

int wMaster::ReapChildren() {
	int iLive = 0;
	for (uint32_t i = 0; i < mWorkerNum; i++) {
        //LOG_DEBUG(ELOG_KEY, "[system] reapchild pid(%d),exited(%d),exiting(%d),detached(%d),respawn(%d)", 
        //	mWorkerPool[i]->mPid, mWorkerPool[i]->mExited, mWorkerPool[i]->mExiting, mWorkerPool[i]->mDetached, mWorkerPool[i]->mRespawn);
        
        if (mWorkerPool[i]->mPid == -1) {
            continue;
        }

        // 已退出
		if (mWorkerPool[i]->mExited) {
			//非分离，就同步文件描述符
			if (!mWorkerPool[i]->mDetached) {
				mWorkerPool[i]->mCh.Close();	//关闭channel
				
				struct ChannelReqClose_t stCh;
				stCh.mFD = -1;
				stCh.mPid = mWorkerPool[i]->mPid;
				stCh.mSlot = i;
				PassCloseChannel(&stCh);
			}
			
			// 重启worker
			if (mWorkerPool[i]->mRespawn && !mWorkerPool[i]->mExiting && !g_terminate && !g_quit) {
				if (SpawnWorker(mWorkerProcessTitle, i) == -1) {
					LOG_ERROR(ELOG_KEY, "[system] could not respawn %d", i);
					continue;
				}
				
				struct ChannelReqOpen_t stCh;
				stCh.mFD = mWorkerPool[mSlot]->mCh[0];
				stCh.mPid = mWorkerPool[mSlot]->mPid;
				stCh.mSlot = i;
				PassOpenChannel(&stCh);
				
				iLive = 1;
				continue;
			}
			
            if (i != (mWorkerNum - 1)) {
                mWorkerPool[i]->mPid = -1;
            } else {
            	//mWorkerNum--;
            }
		} else if (mWorkerPool[i]->mExiting || !mWorkerPool[i]->mDetached) {
			iLive = 1;
		}
    }

	return iLive;
}

wStatus WorkerStart(uint32_t n, int8_t type) {
	struct ChannelReqOpen_t opench;
	for (uint32_t i = 0; i < mWorkerNum; ++i) {
		// 创建worker进程
		if (!SpawnWorker(kProcessTitle, type).Ok()) {
			return mStatus;
		}
		opench.mSlot = mSlot;
        opench.mPid = mWorkerPool[mSlot]->mPid;
        opench.mFD = mWorkerPool[mSlot]->mCh[0];
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

wStatus wMaster::SpawnWorker(const char* title, int32_t type) {
	if (type >= 0) {
		mSlot = static_cast<uint32_t>(type);
		// 退出原始进程
		// todo
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

	// 填充进程表
	if (!NewWorker(title, mSlot, &mWorkerPool[mSlot]).Ok()) {
		return mStatus;
	}
	wWorker *pWorker = mWorkerPool[mSlot];
	
	// 打开进程间IPC通信channel
	mStatus = pWorker->mChannel->Open();
	if (!mStatus.Ok()) {
		return mStatus;
	}

	// 设置第一个描述符的异步IO通知机制
	// FIOASYNC现已被O_ASYNC标志位取代
	// todo
	u_long on = 1;
    if (ioctl((*pWorker->mChannel)[0], FIOASYNC, &on) == -1) {
    	SAFE_DELETE(mWorkerPool[mSlot]);
    	return mStatus = wStatus::IOError("wMaster::SpawnWorker, ioctl(FIOASYNC) failed", strerror(errno));
    }
    // 设置将要在文件描述符channel[0]上接收SIGIO 或 SIGURG事件信号的进程标识
    if (fcntl((*pWorker->mChannel)[0], F_SETOWN, mPid) == -1) {
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
        mStatus = pWorker->PrepareStart();
        if (!mStatus.Ok()) {
        	return mStatus;
        }
        // 进入worker主循环
        pWorker->Start();
        _exit(0);
    }
    //LOG_INFO(ELOG_KEY, "[system] worker start %s [%d] %d", title, mSlot, pid);
    
    // 主进程master更新进程表
    pWorker->mSlot = mSlot;
    pWorker->mPid = pid;
	pWorker->mExited = 0;
	pWorker->mExiting = 0;
	
	if (type >= 0) {
		return mStatus = wStatus::Nothing();
	}

    switch (type) {
		case kPorcessNoRespawn:
		pWorker->mRespawn = 0;
		pWorker->mJustSpawn = 0;
		pWorker->mDetached = 0;
		break;

		case kPorcessRespawn:
		pWorker->mRespawn = 1;
		pWorker->mJustSpawn = 0;
		pWorker->mDetached = 0;
		break;
		
		case kPorcessJustSpawn:
		pWorker->mRespawn = 0;
		pWorker->mJustSpawn = 1;
		pWorker->mDetached = 0;    	

		case kPorcessJustRespawn:
		pWorker->mRespawn = 1;
		pWorker->mJustSpawn = 1;
		pWorker->mDetached = 0;

		case kPorcessDetached:
		pWorker->mRespawn = 0;
		pWorker->mJustSpawn = 0;
		pWorker->mDetached = 1;
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

void wMaster::ProcessGetStatus() {    
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
        if (WEXITSTATUS(status) == 2 && mWorkerPool[i]->mMaster->mRespawn) {
			//LOG_ERROR(ELOG_KEY, "[system] %s %d exited with fatal code %d and cannot be respawned", process, pid, WTERMSIG(status));
            mWorkerPool[i]->mMaster->mRespawn = 0;
        }
    }
}

}	// namespace hnet
