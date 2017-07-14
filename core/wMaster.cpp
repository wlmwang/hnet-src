
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include "wMaster.h"
#include "wServer.h"
#include "wLogger.h"
#include "wMisc.h"
#include "wSigSet.h"
#include "wSignal.h"
#include "wWorker.h"
#include "wTask.h"
#include "wChannelCmd.h"

namespace hnet {

wMaster::wMaster(const std::string& title, wServer* server) : mPid(getpid()), mTitle(title), mSlot(kMaxProcess), mDelay(0), mSigio(0),
mLive(1), mServer(server), mWorker(NULL), mEnv(wEnv::Default()) {
	assert(mServer != NULL);
    std::string pid_path;
	if (mServer->Config()->GetConf("pid_path", &pid_path) && pid_path.size() > 0) {
		mPidPath = pid_path;
	} else {
		mPidPath = soft::GetPidPath();
	}
	memset(mWorkerPool, 0, sizeof(mWorkerPool));
	mNcpu = sysconf(_SC_NPROCESSORS_ONLN); // CPU核数
	mWorkerNum = mNcpu; // worker默认数量
	mServer->mMaster = this; // 便于server引用master中进程表
}

wMaster::~wMaster() {
    for (uint32_t i = 0; i < kMaxProcess; ++i) {
		SAFE_DELETE(mWorkerPool[i]);
    }
}

int wMaster::PrepareStart() {
	soft::TimeUpdate();
	
    std::string host;
    uint16_t port = 0;
    if (!mServer->Config()->GetConf("host", &host) || !mServer->Config()->GetConf("port", &port)) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::PrepareStart GetConf() failed", "");
    	return -1;
    }

    int ret = PrepareRun();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::PrepareStart PrepareRun() failed", "");
    	return ret;
    }

    // worker数量
    uint32_t worker = 0;
    if (mServer->Config()->GetConf("worker", &worker) && worker > 0) {
    	mWorkerNum = worker;
    }

    // 进程标题
    ret = mServer->Config()->Setproctitle(kMasterTitle, mTitle.c_str());
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::PrepareStart Setproctitle() failed", "");
    	return ret;
    }

    // server预启动，创建 listen socket
    std::string protocol;
    if (!mServer->Config()->GetConf("protocol", &protocol)) {
    	ret = mServer->PrepareStart(host, port);
    } else {
    	std::transform(protocol.begin(), protocol.end(), protocol.begin(), ::toupper);
    	ret = mServer->PrepareStart(host, port, protocol);
    }

    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::PrepareStart PrepareStart() failed", "");
    }
    return ret;
}

int wMaster::SingleStart() {
	int ret = CreatePidFile();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SingleStart CreatePidFile() failed", "");
    	return ret;
    }

    ret = InitSignals();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SingleStart InitSignals() failed", "");
        return ret;
    }
    
    ret = Run();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SingleStart Run() failed", "");
    	return ret;
    }
    
    ret = mServer->SingleStart();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SingleStart SingleStart() failed", "");
    }
    return ret;
}

int wMaster::MasterStart() {
    if (mWorkerNum < 0 || mWorkerNum > kMaxProcess) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart () failed", "workernum invalid");
        return -1;
    }

    // 创建pid && 初始化信号处理器
    int ret = CreatePidFile();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart CreatePidFile() failed", "");
    	return ret;
    }

    ret = InitSignals();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart InitSignals() failed", "");
    	return ret;
    }

    // 信号阻塞
    wSigSet ss;
    ss.EmptySet();
    ss.AddSet(SIGCHLD);
    ss.AddSet(SIGALRM);
    ss.AddSet(SIGIO);
    ss.AddSet(SIGQUIT);	// 立即退出
    ss.AddSet(SIGINT);	// 优雅退出
    ss.AddSet(SIGTERM);	// 优雅退出
    ss.AddSet(SIGHUP);	// 重新读取配置
    ss.AddSet(SIGUSR1);	// 重启服务
    ret = ss.Procmask();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart Procmask() failed", "");
    	return ret;
    }
    
    // 初始化惊群锁
    ret = mServer->InitAcceptMutex();
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart InitAcceptMutex() failed", "");
    	return ret;
    }

    // 初始化进程表
	for (uint32_t i = 0; i < kMaxProcess; i++) {
		if (NewWorker(i, &mWorkerPool[i]) == -1) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart NewWorker() failed", "");
			return -1;
		}

		SAFE_NEW(wChannelSocket(kStConnect), mWorkerPool[i]->mChannel);
		if (!mWorkerPool[i]->mChannel) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart new() failed", error::Strerror(errno).c_str());
			return -1;
		}
	}

    // 启动worker工作进程
    ret = WorkerStart(mWorkerNum, kProcessRespawn);
    if (ret == -1) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::MasterStart WorkerStart() failed", "");
    	return ret;
    }

    // 主进程监听信号
    while (true) {
    	soft::TimeUpdate();
		HandleSignal();
		Run();
    }
    return 0;
}

int wMaster::NewWorker(uint32_t slot, wWorker** ptr) {
    SAFE_NEW(wWorker(mTitle, slot, this), *ptr);
    if (!*ptr) {
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::NewWorker new() failed", "");
    	return -1;
    }
    return 0;
}

int wMaster::WorkerStart(uint32_t n, int32_t type) {
	wChannelReqOpen_t open;
	for (uint32_t i = 0; i < n; ++i) {
		if (SpawnWorker(type) == -1) {	// 启动worker
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::WorkerStart SpawnWorker() failed", "");
			return -1;
		}

		// 向所有已启动worker传递刚启动worker的channel描述符（自身进程除外）
		open.set_slot(mSlot);
		open.set_pid(mWorkerPool[mSlot]->mPid);
		open.set_fd(mWorkerPool[mSlot]->ChannelFD(0));
        std::vector<uint32_t> blackslot(1, mSlot);
        mServer->SyncWorker(reinterpret_cast<char*>(&open), sizeof(open), kMaxProcess, &blackslot);
	}
	return 0;
}

int wMaster::SpawnWorker(int64_t type) {
	if (type >= 0) {
		// 启动指定索引worker进程
		mSlot = static_cast<uint32_t>(type);
	} else {
		// 启动指定类型worker进程
		uint32_t idx;
		for (idx = 0; idx < kMaxProcess; ++idx) {
			if (mWorkerPool[idx]->mPid == -1 || mWorkerPool[idx]->ChannelFD(0) == kFDUnknown || mWorkerPool[idx]->ChannelFD(1) == kFDUnknown) {
				break;
			}
		}
		mSlot = idx;
	}
	if (mSlot >= kMaxProcess || mSlot < 0) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SpawnWorker () failed", "slot invalid");
		return -1;
	}

	// 当前进程
	mWorker = mWorkerPool[mSlot];

	// 打开进程间channel通道
	int ret = mWorker->Channel()->Open();
	if (ret == -1) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SpawnWorker Channel.Open() failed", "");
		return ret;
	}

	mWorker->Channel()->SS() = kSsConnected;

    pid_t pid = fork();
    switch (pid) {
    case -1:
		// 关闭channel
    	ret = mWorker->Channel()->Close();
    	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SpawnWorker fork() failed", error::Strerror(errno).c_str());
    	return ret;

    case 0:
    	mWorker->mPid = getpid();

        // worker预启动
        ret = mWorker->PrepareStart();
        if (ret == -1) {
        	exit(2);
        }

        // 进入worker主循环
        ret = mWorker->Start();
        if (ret == -1) {
        	exit(2);
        } else {
        	exit(0);
        }
    }

    // 主进程master
    mWorker->mPid = pid;
    mWorker->mExited = 0;
    mWorker->mTimeline = soft::TimeUnix();
    
	if (type >= 0) {
		return 0;
	}

	mWorker->mExiting = 0;

    switch (type) {
	case kProcessNoRespawn:
		mWorker->mRespawn = 0;
		mWorker->mJustSpawn = 0;
		mWorker->mDetached = 0;
		break;

	case kProcessRespawn:
		mWorker->mRespawn = 1;
		mWorker->mJustSpawn = 0;
		mWorker->mDetached = 0;
		break;

	case kProcessJustSpawn:
		mWorker->mRespawn = 0;
		mWorker->mJustSpawn = 1;
		mWorker->mDetached = 0;
		break;

	case kProcessJustRespawn:
		mWorker->mRespawn = 1;
		mWorker->mJustSpawn = 1;
		mWorker->mDetached = 0;
		break;

	case kProcessDetached:
		mWorker->mRespawn = 0;
		mWorker->mJustSpawn = 0;
		mWorker->mDetached = 1;
		break;
    }
    return 0;
}

int wMaster::PrepareRun() {
    return 0;
}

int wMaster::Run() {
    return 0;
}

int wMaster::Reload() {
	return 0;
}

void wMaster::ProcessExit() {
	return;
}

int wMaster::HandleSignal() {
	if (mDelay) {
		if (g_sigalrm) {
			mSigio = 0;
			mDelay *= 2;
			g_sigalrm = 0;
		}

		struct itimerval itv;
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = mDelay / 1000;
		itv.it_value.tv_usec = (mDelay % 1000 ) * 1000;

		// 设置定时器，以系统真实时间来计算，送出SIGALRM信号（主要用户优雅退出）
		if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::HandleSignal setitimer() failed", error::Strerror(errno).c_str());
			return -1;
		}
	}

	// 阻塞方式等待信号量，定时器控制超时
	wSigSet ss;
	ss.EmptySet();
	ss.Suspend();
	
	// SIGCHLD
	if (g_reap) {
		g_reap = 0;

		WorkerExitStat();	// 进程退出状态
		ReapChildren();		// 重启退出的worker
	}
	
	// 所有worker退出，且收到了退出信号，则master退出
	if (!mLive && (g_terminate || g_quit)) {
	    ProcessExit();
	    if (mServer) {
		    mServer->CleanListenSock();
		    mServer->DeleteAcceptFile();
	    }
	    DeletePidFile();
	    exit(0);
	}
	
	// SIGTERM、SIGINT
	if (g_terminate) {
		// 设置延时，50ms
		if (mDelay == 0) {
			mDelay = 50;
		}
		if (mSigio > 0) {
			mSigio--;
			return 0;
		}
		mSigio = mWorkerNum;

		// 通知所有worker退出，并且等待worker退出
		if (mDelay > 1000) {
			// 延时已到（最大1s），给所有worker发送SIGKILL信号，强制杀死worker
			SignalWorker(SIGKILL);
		} else {
			// 给所有worker发送SIGTERM信号，通知worker优雅退出
			SignalWorker(SIGTERM);
		}
		return 0;
	}

	// SIGQUIT
	if (g_quit) {
		// 给所有的worker发送SIGQUIT信号
		SignalWorker(SIGQUIT);
		return 0;
	}
	
	// SIGHUP
	if (g_reconfigure) {
		g_reconfigure = 0;
		
		// 重新初始化主进程配置
		int ret = Reload();
		if (ret == -1) {
			SignalWorker(SIGQUIT);	// 关闭所有worker进程

			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::HandleSignal Reload() failed", "");
			return ret;
		}

		WorkerStart(mWorkerNum, kProcessJustRespawn);	// 启动新worker

		usleep(100*1000);	// !进程启动100ms
		
		mLive = 1;

		SignalWorker(SIGQUIT);	// 关闭原来worker进程
		return 0;
	}
	return 0;
}

int wMaster::ReapChildren() {
	mLive = 0;
	for (uint32_t i = 0; i < kMaxProcess; i++) {
        if (mWorkerPool[i]->mPid == -1) {
            continue;
        }

        // 进程已退出
		if (mWorkerPool[i]->mExited) {
			if (!mWorkerPool[i]->mDetached) {
				// 关闭channel && 释放进程表项
				mWorkerPool[i]->Channel()->Close();

				// 向新启动的进程传递刚被关闭的进程close消息
				wChannelReqClose_t close;
				close.set_slot(i);
				close.set_pid(mWorkerPool[i]->mPid);
				std::vector<uint32_t> blackslot;
				for (uint32_t n = 0; n < kMaxProcess; n++) {
					if (mWorkerPool[n]->mExited || mWorkerPool[n]->mPid == -1 || mWorkerPool[n]->ChannelFD(0) == kFDUnknown) {
						blackslot.push_back(n);
					}
				}
				mServer->SyncWorker(reinterpret_cast<char*>(&close), sizeof(close), kMaxProcess, &blackslot);
			}
			
			// 非重启命令时，且有进程异常退出，重启
			if (mWorkerPool[i]->mRespawn && !mWorkerPool[i]->mExiting && !g_terminate && !g_quit) {
				if (SpawnWorker(i) == -1) {
					continue;
				}
				// 向所有已启动worker传递刚启动worker的channel描述符
				wChannelReqOpen_t open;
				open.set_slot(i);
				open.set_pid(mWorkerPool[i]->mPid);
				open.set_fd(mWorkerPool[i]->ChannelFD(0));
				std::vector<uint32_t> blackslot(1, i);
				mServer->SyncWorker(reinterpret_cast<char*>(&open), sizeof(open), kMaxProcess, &blackslot);
				mLive = 1;
				continue;
			}

			mWorkerPool[i]->mPid = -1;

		} else if (mWorkerPool[i]->mExiting || !mWorkerPool[i]->mDetached) {
			// 进程正在退出，且不为分离进程，则总认为有存活进程
			mLive = 1;
		}
    }
    return 0;
}

void wMaster::SignalWorker(int signo) {
	wChannelReqQuit_t quit;
	wChannelReqTerminate_t terminate;
	wChannelReqCmd_s* channel = NULL;
	
	if (signo == SIGQUIT) {
		quit.set_pid(mPid);
		channel = &quit;
	} else if (signo == SIGTERM) {
		terminate.set_pid(mPid);
		channel = &terminate;
	}
	
	for (uint32_t i = 0; i < kMaxProcess; i++) {
        if (mWorkerPool[i]->mDetached || mWorkerPool[i]->mPid == -1) {	// 分离进程
        	continue;
        }

        if (mWorkerPool[i]->mJustSpawn) {	// 进程正在重启(重启命令)
        	mWorkerPool[i]->mJustSpawn = 0;	// 只标记一次
        	continue;
        }

        if (mWorkerPool[i]->mExiting && signo == SIGQUIT) {	// 正在退出（优雅退出）
			continue;
		}
		
        if (!channel) {
        	
	        /* TODO: EAGAIN */
	        if (mServer->SyncWorker(reinterpret_cast<char*>(channel), sizeof(*channel), i) == 0) {
        		mWorkerPool[i]->mExiting = 1;
				continue;
	        }
		}

        if (kill(mWorkerPool[i]->mPid, signo) == -1) {
            if (errno == ESRCH) {	// 进程或者线程不存在
                mWorkerPool[i]->mExited = 1;
                mWorkerPool[i]->mExiting = 0;
				g_reap = 1;
            }
            continue;
        }
		
		// 非重启服务
        if (signo != SIGUSR1) {
        	mWorkerPool[i]->mExiting = 1;
        }
    }
}

int wMaster::CreatePidFile() {
	std::string pidstr = logging::NumberToString(mPid);
	return WriteStringToFile(mEnv, pidstr, mPidPath);
}

int wMaster::DeletePidFile() {
	return mEnv->DeleteFile(mPidPath);
}

int wMaster::SignalProcess(const std::string& signal) {
	std::string str;
	if (ReadFileToString(mEnv, mPidPath, &str) != 0) {
		H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SignalProcess ReadFileToString() failed", "");
		return -1;
	}

	uint64_t pid = 0;
	if (logging::DecimalStringToNumber(str, &pid) && pid > 0) {
		for (wSignal::Signal_t* s = g_signals; s->mSigno != 0; ++s) {
			if (memcmp(signal.c_str(), s->mName, signal.size()) == 0) {
				if (kill(pid, s->mSigno) == -1) {
					H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SignalProcess kill() failed", error::Strerror(errno).c_str());
					return -1;
				} else {
					return 0;
				}
			}
		}
	}

	H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::SignalProcess signal() failed", "unfound signal");
	return -1;
}

int wMaster::InitSignals() {
	wSignal signal;
	for (wSignal::Signal_t* s = g_signals; s->mSigno != 0; ++s) {
		if (signal.AddHandler(s) == -1) {
			H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wMaster::InitSignals AddHandler() failed", "");
			return -1;
		}
	}
	return 0;
}

void wMaster::WorkerExitStat() {
    while (true) {
		int status, one = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid == 0) {
            return;
        } else if (pid == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == ECHILD && one) {
                return;
            } else if (errno == ECHILD) {
                return;
            }
            return;
        }
		
        one = 1;
		uint32_t i;
		for (i = 0; i < kMaxProcess; ++i) {
			if (mWorkerPool[i]->mPid == pid) {	// 设置退出状态
                mWorkerPool[i]->mStat = status;
                mWorkerPool[i]->mExited = 1;
                
				// 释放已停止进程，惊群锁
				mServer->ReleaseAcceptMutex(pid);
                break;
			}
		}

		// 日志记录
		std::string str = "wMaster::WorkerExitStat, child(" + logging::NumberToString(pid) + ") ";
        if (WTERMSIG(status)) {
			str += "exited on signal";
			H_LOG_DEBUG(soft::GetLogPath(), "%s : %s", str.c_str(), logging::NumberToString(WTERMSIG(status)).c_str());
        } else {
			str += "exited with code";
			H_LOG_DEBUG(soft::GetLogPath(), "%s : %s", str.c_str(), logging::NumberToString(WEXITSTATUS(status)).c_str());
        }
	
		// 退出码为2时，退出后不重启
        if (WEXITSTATUS(status) == 2 && mWorkerPool[i]->mRespawn) {
        	mWorkerPool[i]->mRespawn = 0;
        	str += ";exited with fatal code, and cannot be respawned";
        	H_LOG_DEBUG(soft::GetLogPath(), "%s : %s", str.c_str(), logging::NumberToString(WEXITSTATUS(status)).c_str());
        }
    }
    return;
}

}	// namespace hnet
