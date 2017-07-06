
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/mman.h>
#include "wFile.h"
#include "wEnv.h"
#include "wMutex.h"
#include "wAtomic.h"
#include "wLogger.h"

namespace hnet {

int wPosixSequentialFile::Read(size_t n, wSlice* result, char* scratch) {
    size_t r = fread(scratch, 1, n, mFile);
    *result = wSlice(scratch, r);
    if (r < n) {
        if (!feof(mFile)) {
            H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSequentialFile::Read feof() failed", error::Strerror(errno).c_str());
            return -1;
        }
    }
    return 0;
}

int wPosixSequentialFile::Skip(uint64_t n) {
    int ret = fseek(mFile, n, SEEK_CUR);
    if (ret != 0) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixSequentialFile::Skip fseek() failed", error::Strerror(errno).c_str());
    }
    return ret;
}


int wPosixRandomAccessFile::Read(uint64_t offset, size_t n, wSlice* result, char* scratch) {
    ssize_t r = pread(mFD, scratch, n, static_cast<off_t>(offset));
    *result = wSlice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixRandomAccessFile::Read pread() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

bool wMmapLimiter::Acquire() {
    if (GetAllowed() <= 0) {
        return false;
    }
    wMutexWrapper l(&mMutex);
    intptr_t x = GetAllowed();
    if (x <= 0) {
        return false;
    } else {
        SetAllowed(x - 1);
        return true;
    }
}

wPosixMmapReadableFile::~wPosixMmapReadableFile() {
    munmap(mMmappedRegion, mLength);
    mLimiter->Release();
}

int wPosixMmapReadableFile::Read(uint64_t offset, size_t n, wSlice* result, char* scratch) {
    if (offset + n > mLength) {
        *result = wSlice();
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixMmapReadableFile::Read n failed", error::Strerror(errno).c_str());
        return -1;
    } else {
        *result = wSlice(reinterpret_cast<char*>(mMmappedRegion) + offset, n);
    }
    return 0;
}


int wPosixWritableFile::Append(const wSlice& data) {
    size_t r = fwrite(data.data(), 1, data.size(), mFile);
    if (r != data.size()) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixWritableFile::Append fwrite() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

int wPosixWritableFile::Close() {
    int ret = fclose(mFile);
    if (ret != 0) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixWritableFile::Close fclose() failed", error::Strerror(errno).c_str());
        return ret;
    }
    mFile = NULL;
    return ret;
}

int wPosixWritableFile::Flush() {
    int ret = fflush(mFile);
    if (ret != 0) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixWritableFile::Close fflush() failed", error::Strerror(errno).c_str());
    }
    return ret;
}

// 刷新数据到文件
int wPosixWritableFile::Sync() {
    if (fflush(mFile) != 0 || fdatasync(fileno(mFile)) != 0) {
        H_LOG_ERROR(soft::GetLogPath(), "%s : %s", "wPosixWritableFile::Close fflush() or fdatasync() failed", error::Strerror(errno).c_str());
        return -1;
    }
    return 0;
}

// 写字符到文件接口
static int DoWriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname, bool should_sync) {
    wWritableFile* file;
    int ret = env->NewWritableFile(fname, &file);
    if (ret == -1) {
        return ret;
    }
    ret = file->Append(data);

    if (ret == 0 && should_sync) {
        ret = file->Sync();
    }
    if (ret == 0) {
        ret = file->Close();
    }
    SAFE_DELETE(file);

    if (ret != 0) {
        env->DeleteFile(fname);
    }
    return ret;
}

// 异步写字符到文件
int WriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname) {
    return DoWriteStringToFile(env, data, fname, false);
}

// 同步写入字符到文件
int WriteStringToFileSync(wEnv* env, const wSlice& data, const std::string& fname) {
    return DoWriteStringToFile(env, data, fname, true);
}

// 顺序读文件到data中
int ReadFileToString(wEnv* env, const std::string& fname, std::string* data) {
    data->clear();
    wSequentialFile* file;
    int ret = env->NewSequentialFile(fname, &file);
    if (ret != 0) {
        return ret;
    }

    static const int kBufferSize = 8192;
    char* space;
    SAFE_NEW_VEC(kBufferSize, char, space);
    while (true) {
        wSlice fragment;
        ret = file->Read(kBufferSize, &fragment, space);
        if (ret != 0) {
            break;
        }
        data->append(fragment.data(), fragment.size());
        if (fragment.empty()) {
            break;
        }
    }
    SAFE_DELETE_VEC(space);
    SAFE_DELETE(file);
    return ret;
}

// 文件加/解独占锁
int LockOrUnlock(int fd, bool lock) {
    errno = 0;
    struct flock f;
    memset(&f, 0, sizeof(f));
    f.l_type = (lock ? F_WRLCK : F_UNLCK);  // 写锁（独占锁）
    f.l_whence = SEEK_SET;
    f.l_start = 0;
    f.l_len = 0;
    return fcntl(fd, F_SETLK, &f);
}

}	// namespace hnet
