
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_FILE_H_
#define _W_FILE_H_

#include "wCore.h"
#include "wLog.h"
#include "wNoncopyable.h"

/**
 * 该类主要用于普通文件处理
 */
class wFile : private wNoncopyable
{
	public:				
		//成功则返回0, 失败返回-1, 错误原因存于mErrno
		int Open(int flags = O_RDWR| O_APPEND| O_EXCL, mode_t mode = 644);

		//成功则返回0, 失败返回-1, 错误原因存于errno
		int Close();

		//成功则返回0, 失败返回-1, 错误原因存于errno
		int Unlink();

		//新的偏移量（成功），-1（失败）
		off_t Lseek(off_t offset, int whence = SEEK_SET);

		/**
		 * @param  pBuf 读取到的内容
		 * @param  nbyte 读取大小
		 * @return 成功时，返回实际所读的字节数，0表示已经读到文件的结束了，小于0表示出现了错误（EINTR 说明由中断引起的失败）
		 */
		ssize_t Read(char *pBuf, size_t nbyte, off_t offset);

		/**
		 * @param  pBuf   写入内容
		 * @param  nbytes pBuf长度
		 * @return 成功时返回写的字节数.失败时返回-1（EINTR 说明由中断引起的失败；EPIPE表示网络连接出现了问题，如对方已经关闭了连接）
		 */
		ssize_t Write(const char *pBuf, size_t nbytes, off_t offset);
		
		int FD() { return mFD;}
		struct stat Stat() { return mInfo;}
		mode_t Mode() { return mInfo.st_mode;}
		off_t Size() { return mInfo.st_size;}
		uid_t Uid() { return mInfo.st_uid;}
		string &FileName() { return mFileName;}

	protected:
	    int mErr;
	    int mFD {FD_UNKNOWN};//文件描述符
	    off_t  mOffset {0};	//现在处理到文件何处了
		string  mFileName;	//文件名称
	    struct stat mInfo;	//文件大小等资源信息
};

#endif