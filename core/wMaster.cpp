
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMaster.h"
#include "wEnv.h"
#include "wSlice.h"
#include "wMisc.h"
#include "wSigSet.h"
#include "wSignal.h"
#include "wWorker.h"
#include "wTask.h"
#include "wChannel.pb.h"

namespace hnet {

wMaster::wMaster(const std::string& title, wServer* server) : mServer(server), mPid(getpid()),
mTitle(title), mSlot(kMaxProcess), mDelay(0), mSigio(0), mLive(1) {
    std::string pid_path;
	if (mServer->Config()->GetConf("pid_path", &pid_path) && pid_path.size() > 0) {
		mPidPath = pid_path;
	} else {
		mPidPath = kPidPath;
	}
	mNcpu = sysconf(_SC_NPROCESSORS_ONLN);
	mWorkerNum = 2*mNcpu;
	memset(mWorkerPool, 0, sizeof(mWorkerPool));
	mServer->Master() = this;
}

wMaster::~wMaster() {
    for (uint32_t i = 0; i < kMaxProcess; ++i) {
		SAFE_DELETE(mWorkerPool[i]);
    }
}

const wStatus& wMaster::PrepareStart() {
    // 检测配置、服务实例
    if (mServer == NULL || mServer->Config() == NULL) {
    	return mStatus = wStatus::Corruption("wMaster::PrepareStart failed", "mServer or mConfig is null");
    }

    std::string host;
    int16_t port = 0;
    if (!mServer->Config()->GetConf("host", &host) || !mServer->Config()->GetConf("port", &port)) {
    	return mStatus = wStatus::Corruption("wMaster::PrepareStart failed", "host or port is illegal");
    }

    if (!PrepareRun().Ok()) {
    	return mStatus;
    }
    if (!(mStatus = mServer->Config()->Setproctitle(kMasterTitle, mTitle.c_str())).Ok()) {
    	return mStatus;
    }

    std::string protocol;
    if (!mServer->Config()->GetConf("protocol", &protocol)) {
    	mStatus = mServer->PrepareStart(host, port);
    } else {
    	mStatus = mServer->PrepareStart(host, port, protocol);
    }
    return mStatus;
}

const wStatus& wMaster::SingleStart() {
    if (!CreatePidFile().Ok()) {
		return mStatus;
    } else if (!InitSignals().Ok()) {
        return mStatus;
    }
    
    if (!Run().Ok()) {
    	return mStatus;
    }
    return mStatus = mServer->SingleStart();
}

const wStatus& wMaster::MasterStart() {
    if (mWorkerNum > kMaxProcess) {
        return mStatus = wStatus::Corruption("wMaster::MasterStart, processes can be spawned", "worker number is overflow");
    }

    if (!CreatePidFile().Ok()) {
		return mStatus;
    } else if (!InitSignals().Ok()) {
        return mStatus;
    }

    // 信号阻塞
    wSigSet ss;
    ss.AddSet(SIGCHLD);
    ss.AddSet(SIGALRM);
    ss.AddSet(SIGIO);
    ss.AddSet(SIGQUIT);	// 立即退出
    ss.AddSet(SIGINT);	// 优雅退出
    ss.AddSet(SIGTERM);	// 优雅退出
    ss.AddSet(SIGHUP);	// 重新读取配置
    ss.AddSet(SIGUSR1);	// 重启服务
    if (!(mStatus = ss.Procmask()).Ok()) {
        return mStatus;
    }

    // 初始化进程表
	for (uint32_t i = 0; i < kMaxProcess; i++) {
		if (!NewWorker(i, &mWorkerPool[i]).Ok()) {
			return mStatus;
		}
	}

    // 启动worker工作进程
    if (!WorkerStart(mWorkerNum, kProcessRespawn).Ok()) {
    	return mStatus;
    }

    // 主进程监听信号
    while (true) {
		HandleSignal();
		Run();
    }
}

const wStatus& wMaster::NewWorker(uint32_t slot, wWorker** ptr) {
    SAFE_NEW(wWorker(mTitle, slot, this), *ptr);
    if (*ptr == NULL) {
		return mStatus = wStatus::IOError("wMaster::NewWorker", "new failed");
    }
    return mStatus.Clear();
}

const wStatus& wMaster::WorkerStart(uint32_t n, int32_t type) {
	wChannelOpen open;
	for (uint32_t i = 0; i < n; ++i) {
		// 启动worker
		if (!SpawnWorker(type).Ok()) {
			return mStatus;
		}

		// 0.5ms延迟
		usleep(500);

		// 向所有已启动worker传递刚启动worker的channel描述符
		open.set_slot(mSlot);
		open.set_pid(mWorkerPool[mSlot]->mPid);
		open.set_fd(mWorkerPool[mSlot]->ChannelFD(0));
        std::vector<uint32_t> blacksolt(1, mSlot);
        mServer->NotifyWorker(&open, kMaxProcess, &blacksolt);
	}
	return mStatus.Clear();
}

const wStatus& wMaster::SpawnWorker(int64_t type) {
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
	if (mSlot >= kMaxProcess) {
		return mStatus = wStatus::Corruption("wMaster::SpawnWorker failed", "slot overflow");
	}
	wWorker *worker = mWorkerPool[mSlot];

	// 打开进程间channel通道
	if (worker->ChannelFD(0) != kFDUnknown || worker->ChannelFD(1) != kFDUnknown) {
		worker->Channel()->Close();
	}
	if (!(mStatus = worker->Channel()->Open()).Ok()) {
		return mStatus;
	}
	worker->Channel()->SS() = kSsConnected;

    pid_t pid = fork();
    switch (pid) {
    case -1:
		// 关闭channel && 释放进程表项
    	worker->Channel()->Close();
        return mStatus = wStatus::Corruption("wMaster::SpawnWorker, fork() failed", strerror(errno));

    case 0:
    	// worker进程
        worker->mPid = getpid();
        if (!(mStatus = worker->Prepare()).Ok()) {
        	// 启动失败
        	exit(2);
        }
        // 进入worker主循环
        mStatus = worker->Start();
        exit(0);
    }

    // 主进程master更新进程表
    worker->mPid = pid;
	worker->mExited = 0;

	if (type >= 0) {
		return mStatus.Clear();
	}

	worker->mExiting = 0;

    switch (type) {
	case kProcessNoRespawn:
		worker->mRespawn = 0;
		worker->mJustSpawn = 0;
		worker->mDetached = 0;
		break;

	case kProcessRespawn:
		worker->mRespawn = 1;
		worker->mJustSpawn = 0;
		worker->mDetached = 0;
		break;

	case kProcessJustSpawn:
		worker->mRespawn = 0;
		worker->mJustSpawn = 1;
		worker->mDetached = 0;
		break;

	case kProcessJustRespawn:
		worker->mRespawn = 1;
		worker->mJustSpawn = 1;
		worker->mDetached = 0;
		break;

	case kProcessDetached:
		worker->mRespawn = 0;
		worker->mJustSpawn = 0;
		worker->mDetached = 1;
		break;
    }

    return mStatus.Clear();
}

const wStatus& wMaster::PrepareRun() {
    return mStatus;
}

const wStatus& wMaster::Run() {
    return mStatus;
}

const wStatus& wMaster::Reload() {
	return mStatus;
}

const wStatus& wMaster::HandleSignal() {
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
			mStatus = wStatus::Corruption("wMaster::HandleSignal, setitimer() failed", strerror(errno));
		}
	}

	// 阻塞方式等待信号量，定时器控制超时
	wSigSet ss;
	ss.Suspend();
	
	// SIGCHLD
	if (g_reap) {
		g_reap = 0;

		WorkerExitStat();

		// 重启退出的worker
		ReapChildren();
	}
	
	// 所有worker退出，且收到了退出信号，则master退出
	if (!mLive && (g_terminate || g_quit)) {
		ProcessExit();
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
			return mStatus;
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
		return mStatus;
	}

	// SIGQUIT
	if (g_quit) {
		// 给所有的worker发送SIGQUIT信号
		SignalWorker(SIGQUIT);
		return mStatus;
	}
	
	// SIGHUP
	if (g_reconfigure) {
		g_reconfigure = 0;
		
		// 重新初始化主进程配置
		if (!Reload().Ok()) {
			exit(2);
		}
		
		// 启动新worker
		WorkerStart(mWorkerNum, kProcessJustRespawn);
		
		// 100ms
		usleep(100*1000);
		
		mLive = 1;

		// 关闭原来worker进程
		SignalWorker(SIGTERM);
	}
	return mStatus;
}

const wStatus& wMaster::ReapChildren() {
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
				wChannelClose close;
				close.set_slot(i);
				close.set_pid(mWorkerPool[i]->mPid);
				for (uint32_t n = 0; n < kMaxProcess; n++) {
					if (mWorkerPool[n]->mExited || mWorkerPool[n]->mPid == -1 || mWorkerPool[n]->ChannelFD(0) == kFDUnknown) {
						continue;
					}
					mServer->NotifyWorker(&close, n);
				}
			}
			
			// 重启worker
			if (mWorkerPool[i]->mRespawn && !mWorkerPool[i]->mExiting && !g_terminate && !g_quit) {
				if (!SpawnWorker(i).Ok()) {
					continue;
				}
				// 0.5ms延迟
				usleep(500);

				// 向所有已启动worker传递刚启动worker的channel描述符
				wChannelOpen open;
				open.set_slot(i);
				open.set_pid(mWorkerPool[i]->mPid);
				open.set_fd(mWorkerPool[i]->ChannelFD(0));

				std::vector<uint32_t> blacksolt(1, i);
				mServer->NotifyWorker(&open, kMaxProcess, &blacksolt);

				mLive = 1;
				continue;
			}

			mWorkerPool[i]->mPid = -1;

		} else if (mWorkerPool[i]->mExiting || !mWorkerPool[i]->mDetached) {
			// 进程正在退出，且不为分离进程，则总认为有存活进程
			mLive = 1;
		}
    }
    return mStatus.Clear();
}

void wMaster::SignalWorker(int signo) {
	wChannelQuit quit;
	wChannelTerminate terminate;
	const google::protobuf::Message* channel = NULL;

	if (signo == SIGQUIT) {
		quit.set_pid(mPid);
		channel = &quit;
	} else if (signo == SIGTERM) {
		terminate.set_pid(mPid);
		channel = &terminate;
	}
	
	for (uint32_t i = 0; i < kMaxProcess; i++) {
        if (mWorkerPool[i]->mDetached || mWorkerPool[i]->mPid == -1) {
        	// 分离进程
        	continue;
        } else if (mWorkerPool[i]->mJustSpawn) {
        	// 进程正在重启(重启命令)
        	mWorkerPool[i]->mJustSpawn = 0;
        	continue;
        } else if (mWorkerPool[i]->mExiting && signo == SIGQUIT) {
			// 正在退出
			continue;
		}
		
        if (channel != NULL) {

	        /* TODO: EAGAIN */
        	if (mServer->NotifyWorker(channel, i).Ok()) {
        		mWorkerPool[i]->mExiting = 1;
				continue;
        	}
		}

        if (kill(mWorkerPool[i]->mPid, signo) == -1) {
            if (errno == ESRCH) {
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

const wStatus& wMaster::CreatePidFile() {
	std::string pidstr = logging::NumberToString(mPid);
	return mStatus = hnet::WriteStringToFile(wEnv::Default(), pidstr, mPidPath);
}

const wStatus& wMaster::DeletePidFile() {
	return mStatus = wEnv::Default()->DeleteFile(mPidPath);
}

const wStatus& wMaster::SignalProcess(const std::string& signal) {
	std::string str;
	if (!(mStatus = hnet::ReadFileToString(wEnv::Default(), mPidPath, &str)).Ok()) {
		return mStatus;
	}

	uint64_t pid = 0;
	if (logging::DecimalStringToNumber(str, &pid) && pid > 0) {
		for (wSignal::Signal_t* s = g_signals; s->mSigno != 0; ++s) {
			if (memcmp(signal.c_str(), s->mName, signal.size()) == 0) {
				if (kill(pid, s->mSigno) == -1) {
					return mStatus = wStatus::Corruption("wMaster::SignalProcess, kill() failed", strerror(errno));
				} else {
					return mStatus.Clear();
				}
			}
		}
	}
	return mStatus = wStatus::Corruption("wMaster::SignalProcess, signal failed", "cannot find signal");
}

const wStatus& wMaster::InitSignals() {
	wSignal signal;
	for (wSignal::Signal_t* s = g_signals; s->mSigno != 0; ++s) {
		if (!(mStatus = signal.AddHandler(s)).Ok()) {
			return mStatus;
		}
	}
	return mStatus;
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
			if (mWorkerPool[i]->mPid == pid) {
				// 设置退出状态
                mWorkerPool[i]->mStat = status;
                mWorkerPool[i]->mExited = 1;
                break;
			}
		}

		// 日志记录
		std::string str = "wMaster::WorkerExitStat, child(" + logging::NumberToString(pid) + ") ";
        if (WTERMSIG(status)) {
			wStatus::Corruption(str + "exited on signal", logging::NumberToString(WTERMSIG(status)));
        } else {
			wStatus::Corruption(str + "exited with code", logging::NumberToString(WTERMSIG(status)));
        }
	
		// 退出码为2时，退出后不重启
        if (WEXITSTATUS(status) == 2 && mWorkerPool[i]->mRespawn) {
        	mWorkerPool[i]->mRespawn = 0;
        	wStatus::Corruption(str + "exited with fatal code, and cannot be respawned", logging::NumberToString(WTERMSIG(status)));
        }
    }
}

}	// namespace hnet
