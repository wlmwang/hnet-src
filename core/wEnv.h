
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ENV_H_
#define _W_ENV_H_

#include <string>
#include <deque>
#include <set>
#include <vector>
#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

class wSlice;

// 顺序文件接口
class wSequentialFile : private wNoncopyable {
public:
    wSequentialFile() { }
    virtual ~wSequentialFile() { }

    // 读取n字节到result中，scratch为用户传入缓冲区
    // 要求：外部实现同步操作
    virtual wStatus Read(size_t n, wSlice* result, char* scratch) = 0;

    // 跳过n字节
    // 要求：外部实现同步操作
    virtual wStatus Skip(uint64_t n) = 0;
};

// 随机文件访问接口
class wRandomAccessFile : private wNoncopyable {
public:
    wRandomAccessFile() { }
    virtual ~wRandomAccessFile() { }

    // offset处读取n字节字符到result中，scratch为用户传入缓冲区
    // 线程安全函数
    virtual wStatus Read(uint64_t offset, size_t n, wSlice* result, char* scratch) const = 0;
};

// 写文件类
class wWritableFile : private wNoncopyable {
public:
    wWritableFile() { }
    virtual ~wWritableFile() { }

    virtual wStatus Append(const wSlice& data) = 0;
    virtual wStatus Close() = 0;
    virtual wStatus Flush() = 0;
    virtual wStatus Sync() = 0;
};

// 文件锁（句柄）
class wFileLock : private wNoncopyable {
public:
    wFileLock() { }
    virtual ~wFileLock() { }
};


class wEnv : private wNoncopyable {
public:
    wEnv() { }
    virtual ~wEnv() { }

    // 单例
    static wEnv* Default();

    // 创建顺序文件访问对象
    virtual wStatus NewSequentialFile(const std::string& fname, wSequentialFile** result) = 0;
    // 创建随机访问文件对象
    virtual wStatus NewRandomAccessFile(const std::string& fname, wRandomAccessFile** result) = 0;

    // 创建写入文件对象
    virtual wStatus NewWritableFile(const std::string& fname, wWritableFile** result) = 0;

    virtual bool FileExists(const std::string& fname) = 0;

    // 返回目录下所有文件、目录名
    virtual wStatus GetChildren(const std::string& dir, std::vector<std::string>* result) = 0;

    virtual wStatus DeleteFile(const std::string& fname) = 0;

    // 创建目录
    virtual wStatus CreateDir(const std::string& dirname) = 0;

    // 删除目录
    virtual wStatus DeleteDir(const std::string& dirname) = 0;

    // Store the size of fname in *file_size.
    virtual wStatus GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

    // Rename file src to target.
    virtual wStatus RenameFile(const std::string& src, const std::string& target) = 0;

    // 锁文件
    virtual wStatus LockFile(const std::string& fname, wFileLock** lock) = 0;

    // 解锁文件
    // 要求：先调用 LockFile 成功
    // 要求：锁住还未锁定成功
    virtual wStatus UnlockFile(wFileLock* lock) = 0;

    // 添加任务到后台任务消费线程中
    virtual void Schedule(void (*function)(void* arg), void* arg) = 0;

    // 开启新线程
    virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

    // 返回日志对象
    //virtual wStatus NewLogger(const std::string& fname, wLogger** result) = 0;

    virtual uint64_t NowMicros() = 0;

    // Sleep/delay the thread for the prescribed number of micro-seconds.
    virtual void SleepForMicroseconds(int micros) = 0;
};

extern wStatus WriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname);
extern wStatus ReadFileToString(wEnv* env, const std::string& fname, std::string* data);

// env包装类
class wEnvWrapper : public wEnv {
public:
    explicit wEnvWrapper(wEnv* t) : mTarget(t) { }
    virtual ~wEnvWrapper() { }

    wEnv* target() const { return mTarget; }

    wStatus NewSequentialFile(const std::string& f, wSequentialFile** r) {
        return mTarget->NewSequentialFile(f, r);
    }

    wStatus NewRandomAccessFile(const std::string& f, wRandomAccessFile** r) {
        return mTarget->NewRandomAccessFile(f, r);
    }

    wStatus NewWritableFile(const std::string& f, wWritableFile** r) {
        return mTarget->NewWritableFile(f, r);
    }

    bool FileExists(const std::string& f) {
        return mTarget->FileExists(f); 
    }

    wStatus GetChildren(const std::string& dir, std::vector<std::string>* r) {
        return mTarget->GetChildren(dir, r);
    }

    wStatus DeleteFile(const std::string& f) { 
        return mTarget->DeleteFile(f); 
    }

    wStatus CreateDir(const std::string& d) { 
        return mTarget->CreateDir(d); 
    }

    wStatus DeleteDir(const std::string& d) { 
        return mTarget->DeleteDir(d); 
    }

    wStatus GetFileSize(const std::string& f, uint64_t* s) {
        return mTarget->GetFileSize(f, s);
    }

    wStatus RenameFile(const std::string& s, const std::string& t) {
        return mTarget->RenameFile(s, t);
    }

    wStatus LockFile(const std::string& f, wFileLock** l) {
        return mTarget->LockFile(f, l);
    }

    wStatus UnlockFile(wFileLock* l) { 
        return mTarget->UnlockFile(l); 
    }

    void Schedule(void (*f)(void*), void* a) {
        return mTarget->Schedule(f, a);
    }

    void StartThread(void (*f)(void*), void* a) {
        return mTarget->StartThread(f, a);
    }

    /*
    virtual wStatus NewLogger(const std::string& fname, Logger** result) {
        return mTarget->NewLogger(fname, result);
    }
    */
   
    uint64_t NowMicros() {
        return mTarget->NowMicros();
    }

    void SleepForMicroseconds(int micros) {
        mTarget->SleepForMicroseconds(micros);
    }

private:
    wEnv* mTarget;
};

}	// namespace hnet

#endif