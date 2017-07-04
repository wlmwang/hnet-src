
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <pwd.h>
#include <grp.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include "wDaemon.h"
#include "wMisc.h"
#include "wSignal.h"
#include "wLogger.h"

namespace hnet {

int wDaemon::Start(const std::string& lock_path, const char *prefix) {
    if (!lock_path.empty()) {
    	mFilename = lock_path;
    } else {
    	mFilename = soft::GetLockPath();
    }

    // 独占式锁定文件，防止有相同程序的进程已经启动
    int lockFD = open(mFilename.c_str(), O_RDWR|O_CREAT, 0640);
    if (lockFD < 0) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wDaemon::Start open() failed", error::Strerror(errno).c_str());
    	return -1;
    }

    if (flock(lockFD, LOCK_EX | LOCK_NB) < 0) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wDaemon::Start flock() failed", error::Strerror(errno).c_str());
        return -1;
    }

    // 若是以root身份运行，设置进程的实际、有效uid
    if (geteuid() == 0) {
        if (setuid(soft::GetUser()) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wDaemon::Start setuid() failed", error::Strerror(errno).c_str());
            return -1;
        }
        if (setgid(soft::GetGroup()) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wDaemon::Start setgid() failed", error::Strerror(errno).c_str());
            return -1;
        }
    }

    if (fork() != 0) {
        exit(-1);
    }
    setsid();
    
    // 忽略以下信号
    wSignal s;
    s.EmptySet(SIG_IGN);
    s.AddSigno(SIGINT);
    s.AddSigno(SIGHUP);
    s.AddSigno(SIGQUIT);
    s.AddSigno(SIGTERM);
    s.AddSigno(SIGCHLD);
    s.AddSigno(SIGPIPE);
    s.AddSigno(SIGTTIN);
    s.AddSigno(SIGTTOU);

    if (fork() != 0) {
        exit(-1);
    }
    return 0;
}


}	// namespace hnet