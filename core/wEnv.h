
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ENV_H_
#define _W_ENV_H_

#include <vector>
#include "wCore.h"
#include "wStatus.h"
#include "wSlice.h"
#include "wNoncopyable.h"

namespace hnet {

class wSequentialFile;
class wRandomAccessFile;
class wWritableFile;
class wFileLock;
class wLogger;
class wSem;

// 平台类
class wEnv : private wNoncopyable {
public:
    wEnv() { }
    virtual ~wEnv() { }

    // 单例
    static wEnv* Default();

    // 创建顺序文件访问对象
    virtual const wStatus& NewSequentialFile(const std::string& fname, wSequentialFile** result) = 0;

    // 创建随机访问文件对象
    virtual const wStatus& NewRandomAccessFile(const std::string& fname, wRandomAccessFile** result) = 0;

    // 创建写入文件对象
    virtual const wStatus& NewWritableFile(const std::string& fname, wWritableFile** result) = 0;

    // 返回日志对象
    virtual const wStatus& NewLogger(const std::string& fname, wLogger** result, off_t maxsize = 32*1024*1024) = 0;

    // 返回信号量对象，进程间同步
    virtual const wStatus& NewSem(const std::string *devshm, wSem** result) = 0;

    // 锁文件
    virtual const wStatus& LockFile(const std::string& fname, wFileLock** lock) = 0;

    // 解锁文件
    // 要求：先调用 LockFile 成功
    // 要求：锁住还未锁定成功
    virtual const wStatus& UnlockFile(wFileLock* lock) = 0;

    // 返回目录下所有文件、目录名
    virtual const wStatus& GetChildren(const std::string& dir, std::vector<std::string>* result, bool fullname = true) = 0;

    virtual const wStatus& GetRealPath(const std::string& fname, std::string* result) = 0;

    virtual const wStatus& DeleteFile(const std::string& fname) = 0;

    // 创建目录
    virtual const wStatus& CreateDir(const std::string& dirname) = 0;

    // 删除目录
    virtual const wStatus& DeleteDir(const std::string& dirname) = 0;

    // 文件大小
    virtual const wStatus& GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

    // 重命名文件名
    virtual const wStatus& RenameFile(const std::string& src, const std::string& target) = 0;

    virtual bool FileExists(const std::string& fname) = 0;

    // 添加任务到后台任务消费线程中
    virtual void Schedule(void (*function)(void* arg), void* arg) = 0;

    // 开启新线程
    virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

    // 返回微妙
    virtual uint64_t NowMicros() = 0;

    // sleep/delay线程micros微妙时间
    virtual void SleepForMicroseconds(int micros) = 0;
};

extern wStatus WriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname);
extern wStatus ReadFileToString(wEnv* env, const std::string& fname, std::string* data);

}	// namespace hnet

#endif
