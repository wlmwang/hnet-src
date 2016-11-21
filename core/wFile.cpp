
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <sys/mman.h>
#include "wFile.h"
#include "wEnv.h"
#include "wMutex.h"
#include "wAtomic.h"

namespace hnet {

const wStatus& wPosixSequentialFile::Read(size_t n, wSlice* result, char* scratch) {
    size_t r = fread(scratch, 1, n, mFile);
    *result = wSlice(scratch, r);
    if (r < n) {
        if (!feof(mFile)) {
        	return mStatus = wStatus::IOError(mFilename, strerror(errno));
        }
    }
    return mStatus.Clear();
}

const wStatus& wPosixSequentialFile::Skip(uint64_t n) {
    if (fseek(mFile, n, SEEK_CUR)) {
        return mStatus = wStatus::IOError(mFilename, strerror(errno));
    }
    return mStatus.Clear();
}


const wStatus& wPosixRandomAccessFile::Read(uint64_t offset, size_t n, wSlice* result, char* scratch) {
    ssize_t r = pread(mFD, scratch, n, static_cast<off_t>(offset));
    *result = wSlice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
    	return mStatus = wStatus::IOError(mFilename, strerror(errno));
    }
    return mStatus.Clear();
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

const wStatus& wPosixMmapReadableFile::Read(uint64_t offset, size_t n, wSlice* result, char* scratch) {
    if (offset + n > mLength) {
        *result = wSlice();
        return mStatus = wStatus::IOError(mFilename, strerror(EINVAL));
    } else {
        *result = wSlice(reinterpret_cast<char*>(mMmappedRegion) + offset, n);
    }
    return mStatus.Clear();
}


const wStatus& wPosixWritableFile::Append(const wSlice& data) {
    size_t r = fwrite(data.data(), 1, data.size(), mFile);
    if (r != data.size()) {
        return mStatus = wStatus::IOError(mFilename, strerror(errno));
    }
    return mStatus.Clear();
}

const wStatus& wPosixWritableFile::Close() {
    if (fclose(mFile) != 0) {
        return mStatus = wStatus::IOError(mFilename, strerror(errno));
    }
    mFile = NULL;
    return mStatus.Clear();
}

const wStatus& wPosixWritableFile::Flush() {
    if (fflush(mFile) != 0) {
    	return mStatus = wStatus::IOError(mFilename, strerror(errno));
    }
    return mStatus.Clear();
}

// 刷新数据到文件
const wStatus& wPosixWritableFile::Sync() {
    if (fflush(mFile) != 0 || fdatasync(fileno(mFile)) != 0) {
    	return mStatus = wStatus::IOError(mFilename, strerror(errno));
    }
    return mStatus.Clear();
}

// 写字符到文件接口
static wStatus DoWriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname, bool should_sync) {
    wWritableFile* file;
    wStatus s = env->NewWritableFile(fname, &file);
    if (!s.Ok()) {
        return s;
    }
    s = file->Append(data);

    if (s.Ok() && should_sync) {
        s = file->Sync();
    }
    if (s.Ok()) {
        s = file->Close();
    }
    SAFE_DELETE(file);

    if (!s.Ok()) {
        env->DeleteFile(fname);
    }
    return s;
}

// 异步写字符到文件
wStatus WriteStringToFile(wEnv* env, const wSlice& data, const std::string& fname) {
    return DoWriteStringToFile(env, data, fname, false);
}

// 同步写入字符到文件
wStatus WriteStringToFileSync(wEnv* env, const wSlice& data, const std::string& fname) {
    return DoWriteStringToFile(env, data, fname, true);
}

// 顺序读文件到data中
wStatus ReadFileToString(wEnv* env, const std::string& fname, std::string* data) {
    data->clear();
    wSequentialFile* file;
    wStatus s = env->NewSequentialFile(fname, &file);
    if (!s.Ok()) {
        return s;
    }

    static const int kBufferSize = 8192;
    char* space;
    SAFE_NEW_VEC(kBufferSize, char, space);
    while (true) {
        wSlice fragment;
        s = file->Read(kBufferSize, &fragment, space);
        if (!s.Ok()) {
            break;
        }
        data->append(fragment.data(), fragment.size());
        if (fragment.empty()) {
            break;
        }
    }
    SAFE_DELETE_VEC(space);
    SAFE_DELETE(file);
    return s;
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
