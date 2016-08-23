
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wFile.h"

int wFile::Open(int flags, mode_t mode)
{
	if (mFileName.size() <= 0)
	{
		return -1;
	}

	mFD = open(mFileName.c_str(), flags, mode);
	if (mFD == -1)
	{
		mErr = errno;
		return -1;
	}
	fstat(mFD, &mInfo);
	return mFD;
}

int wFile::Close()
{
	if(close(mFD) == -1)
	{
		mErr = errno;
		return -1;
	}
	return 0;
}

int wFile::Unlink()
{
	if(unlink(mFileName.c_str()) == -1)
	{
		mErr = errno;
		return -1;
	}
	return 0;
}

off_t wFile::Lseek(off_t offset, int whence)
{
	mOffset = lseek(mFD, offset, whence);
	return mOffset;
}

ssize_t wFile::Read(char *pBuf, size_t nbyte, off_t offset)
{
	ssize_t  n;
	n = pread(mFD, pBuf, nbyte, offset);
	if (n == -1)
	{
		mErr = errno;
		LOG_ERROR(ELOG_KEY, "[system] pread() \"%s\" failed", mFileName.c_str());
		return -1;
	}

	mOffset += n;
	return n;
}

ssize_t wFile::Write(const char *pBuf, size_t nbytes, off_t offset)
{
    ssize_t n, written;

    written = 0;
    while (true) 
    {
        n = pwrite(mFD, pBuf + written, nbytes, offset);

        if (n == -1) 
        {
			mErr = errno;

            if (mErr == EINTR) 
            {
                LOG_DEBUG(ELOG_KEY, "[system] pwrite() was interrupted");
                continue;
            }

            LOG_ERROR(ELOG_KEY, "[system] pwrite() \"%s\" failed", mFileName.c_str());
            return -1;
        }

        mOffset += n;
        written += n;

        if ((size_t) n == nbytes) 
        {
            return written;
        }

        offset += n;
        nbytes -= n;
    }
}
		