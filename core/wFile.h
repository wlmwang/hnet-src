
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_FILE_H_
#define _W_FILE_H_

#include <set>
#include <vector>
#include "wCore.h"
#include "wStatus.h"
#include "wSlice.h"
#include "wNoncopyable.h"
#include "wMutex.h"
#include "wAtomic.h"

namespace hnet {

// 顺序访问文件接口
class wSequentialFile : private wNoncopyable {
public:
    wSequentialFile() { }
    virtual ~wSequentialFile() { }

    // 读取n字节到result中，scratch为用户传入缓冲区
    // 要求：外部实现同步操作
    virtual const wStatus& Read(size_t n, wSlice* result, char* scratch) = 0;

    // 跳过n字节
    // 要求：外部实现同步操作
    virtual const wStatus& Skip(uint64_t n) = 0;
};

// 随机访问文件接口
class wRandomAccessFile : private wNoncopyable {
public:
    wRandomAccessFile() { }
    virtual ~wRandomAccessFile() { }

    // offset处读取n字节字符到result中，scratch为用户传入缓冲区
    // 要求：外部实现同步操作
    virtual const wStatus& Read(uint64_t offset, size_t n, wSlice* result, char* scratch) = 0;
};

// 写文件类
class wWritableFile : private wNoncopyable {
public:
    wWritableFile() { }
    virtual ~wWritableFile() { }

    virtual const wStatus& Append(const wSlice& data) = 0;
    virtual const wStatus& Close() = 0;
    virtual const wStatus& Flush() = 0;
    virtual const wStatus& Sync() = 0;
};

// 文件锁接口
class wFileLock : private wNoncopyable {
public:
    wFileLock() { }
    virtual ~wFileLock() { }
};


// 实现类

// 顺序访问文件实现类
class wPosixSequentialFile : public wSequentialFile {
public:
    wPosixSequentialFile(const std::string& fname, FILE* f) : mFilename(fname), mFile(f) { }

    virtual ~wPosixSequentialFile() {
    	fclose(mFile);
    }

    // 要求：外部保证为同步操作
    virtual const wStatus& Read(size_t n, wSlice* result, char* scratch);

    // 跳过指定字节
    virtual const wStatus& Skip(uint64_t n);

private:
    std::string mFilename;
    FILE* mFile;
    wStatus mStatus;
};

// 随机访问文件实现类
class wPosixRandomAccessFile : public wRandomAccessFile {
public:
    wPosixRandomAccessFile(const std::string& fname, int fd) : mFilename(fname), mFD(fd) { }

    virtual ~wPosixRandomAccessFile() {
    	close(mFD);
    }

    // 要求：外部保证同步操作
    virtual const wStatus& Read(uint64_t offset, size_t n, wSlice* result, char* scratch);

private:
    std::string mFilename;
    int mFD;
    wStatus mStatus;
};

// 优化存取文件时，可一次性映射到内存中。64-bit说明共享内存足够大（虚址空间）
class wMmapLimiter : private wNoncopyable {
public:
    // 64-bit可创建1000个mmaps，小于64-bit不能创建mmaps
    wMmapLimiter() {
        SetAllowed(sizeof(void*) >= 8 ? 1000 : 0);
    }

    bool Acquire();

    void Release() {
        wMutexWrapper l(&mMutex);
        SetAllowed(GetAllowed() + 1);
    }
private:
    intptr_t GetAllowed() const {
        return reinterpret_cast<intptr_t>(mAllowed.AcquireLoad());
    }

    // 要求：调用前 mMutex 需加锁
    void SetAllowed(intptr_t v) {
        mAllowed.ReleaseStore(reinterpret_cast<void*>(v));
    }

    wMutex mMutex;
    wAtomic mAllowed;
};

// 基于共享内存随机访问文件实现类
class wPosixMmapReadableFile: public wRandomAccessFile {
public:
    // base[0,length-1] contains the mmapped contents of the file.
    wPosixMmapReadableFile(const std::string& fname, void* base, size_t length, wMmapLimiter* limiter)
    : mFilename(fname), mMmappedRegion(base), mLength(length),mLimiter(limiter) { }

    virtual ~wPosixMmapReadableFile();

    // 要求：外部保证同步操作
    virtual const wStatus& Read(uint64_t offset, size_t n, wSlice* result, char* scratch);

private:
    std::string mFilename;  // 文件名
    void* mMmappedRegion;  // 共享内存起始地址
    size_t mLength;         // 共享内存（文件）长度
    wMmapLimiter* mLimiter; // CPU平台
    wStatus mStatus;
};

// 写文件类
class wPosixWritableFile : public wWritableFile {
public:
    wPosixWritableFile(const std::string& fname, FILE* f) : mFilename(fname), mFile(f) { }

    virtual ~wPosixWritableFile() {
        if (mFile != NULL) {
            fclose(mFile);
        }
    }

    // 要求：外部保证同步操作
    virtual const wStatus& Append(const wSlice& data);

    virtual const wStatus& Close();

    virtual const wStatus& Flush();

    // 刷新数据到文件
    virtual const wStatus& Sync();

private:
    std::string mFilename;
    FILE* mFile;
    wStatus mStatus;
};

// 文件锁句柄
class wPosixFileLock : public wFileLock {
public:
    int mFD;
    std::string mName;
};

// 进程文件锁集合，防止同一个进程多次锁同一文件
class wPosixLockTable {
public:
    bool Insert(const std::string& fname) {
        wMutexWrapper l(&mMutex);
        return mLockedFiles.insert(fname).second;
    }

    void Remove(const std::string& fname) {
        wMutexWrapper l(&mMutex);
        mLockedFiles.erase(fname);
    }

private:
    wMutex mMutex;
    // 文件锁集合
    std::set<std::string> mLockedFiles;
};

extern int LockOrUnlock(int fd, bool lock);

}	// namespace hnet

#endif
