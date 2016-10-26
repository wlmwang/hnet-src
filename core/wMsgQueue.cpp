
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMsgQueue.h"
#include "wMisc.h"

namespace hnet {

void wMsgQueue::SetBuffer(char buf[], int len) {
	char *p = buf;
	// 前4位开始地址
	mBeginIdxPtr = reinterpret_cast<int *>(p);
	p += sizeof(int);
	// 后4位开始地址
	mEndIdxPtr = reinterpret_cast<int *>(p);
	p += sizeof(int);
	// 实际数据地址
	mBufferPtr = p;
	// 实际数据长度
	mQueueSize = len - 2 * sizeof(int);
}

int wMsgQueue::Pop(char buf[], int len) {
	if (buf == NULL) {
		return -1;
	}

	// 临时值，防止发送端同时操作产生同步问题
	int begin = *mBeginIdxPtr;
	int end = *mEndIdxPtr;

	if (begin == end) {
		return 0;
	}

	// 当前所有消息占据的长度
	int msglen;
	if (begin > end) {
		msglen = (int)mQueueSize - begin + end;
	} else {
		msglen = end - begin;
	}
	
	//如果消息有问题，则直接忽略
	if (msglen <= static_cast<int>(sizeof(int))) {
		*mBeginIdxPtr = end;
		return -2;
	}

	//消息长度
	int headlen;
	//分段长度
	if (begin > static_cast<int>(mQueueSize) - static_cast<int>(sizeof(int))) {
		int copeonce = static_cast<int>(mQueueSize - begin);
		memcpy((void *)&headlen, reinterpret_cast<const void *>(mBufferPtr + begin), copeonce);
		memcpy((void *)(((char *)&headlen) + copeonce), reinterpret_cast<const void *>(mBufferPtr), static_cast<int>(sizeof(int)) - copeonce);
	} else {
		headlen = *reinterpret_cast<int *>(mBufferPtr + begin);
	}

	// 消息长度判断
	if (headlen < 0 || headlen > msglen - static_cast<int>(sizeof(int))) {
		return -3;
	}
	
	// 如果接收长度过短
	if (headlen > len) {
		return -4;
	}

	int msgend = begin + static_cast<int>(sizeof(int)) + headlen;
	//如果消息没有分段
	if (msgend <= (int)mQueueSize) {
		memcpy((void *)buf, (const void *)(mBufferPtr + begin + static_cast<int>(sizeof(int))), headlen);
		*mBeginIdxPtr = msgend;
		return headlen;
	} else {
		//前半段的长度
		int templen = (int)mQueueSize - static_cast<int>(sizeof(int)) - begin;
		if (templen <= 0) {
			memcpy((void *)buf, reinterpret_cast<const void*>(mBufferPtr - templen), headlen);
			*mBeginIdxPtr = headlen - templen;
			return headlen;
		}
		//拷贝前半段
		memcpy(reinterpret_cast<const void*>(buf), reinterpret_cast<const void*>(mBufferPtr + begin + (int)sizeof(int)), templen);
		//后半段的长度
		int lastlen = headlen - templen;
		//拷贝后半段
		memcpy((void *)(buf + templen), reinterpret_cast<const void*>(mBufferPtr), lastlen);
		//调整起始位置
		*mBeginIdxPtr = lastlen;
		return headlen;
	}
}

int wMsgQueue::Push(char *buf, int len) {
	if (buf == NULL || len < 0 || len > mQueueSize - kReserveLen) {
		return -1;
	}
	int begin = *mBeginIdxPtr;
	int end = *mEndIdxPtr;

	//剩余的空间长度
	int lastlen;
	if (begin == end) {
		lastlen = (int)mQueueSize;
	} else if (begin > end) {
		lastlen = begin - end;
	} else {
		lastlen = (int)mQueueSize - end + begin;
	}

	//减去保留长度
	lastlen -= kReserveLen;

	//如果剩余长度不够
	if (lastlen < len + static_cast<int>(sizeof(int))) {
		return -2;
	}

	//消息长度判断
	if (len <= 0 || len > mQueueSize - kReserveLen) {
		return -3;
	}
	//拷贝长度，如果分段
	if (end > (int)mQueueSize - (int)sizeof(int)) {
		int iLenCopyOnce = (int)mQueueSize - end;
		memcpy((void *)(mBufferPtr + end), (const void *)&len, iLenCopyOnce);
		memcpy((void *)mBufferPtr, (const void *)(((char *)&len) + iLenCopyOnce), sizeof(int) - iLenCopyOnce);
	} else {	// 如果不分段
		*(int *)(mBufferPtr + end) = len;
	}

	//确定当前的拷贝入口
	int iNowEndIdx = end + (int)sizeof(int);
	if (iNowEndIdx >= (int)mQueueSize) {
		iNowEndIdx -= (int)mQueueSize;
	}

	int msgend = iNowEndIdx + len;
	//不需要分段
	if (msgend <= (int)mQueueSize) {
		memcpy((void *)(mBufferPtr + iNowEndIdx), (const void *)buf, len);
		*mEndIdxPtr = iNowEndIdx + len;
		return 0;
	} else {
		//前半段的长度
		int tmplen = (int)mQueueSize - iNowEndIdx;
		//拷贝消息前半段
		memcpy((void *)(mBufferPtr + iNowEndIdx), (const void *)buf, tmplen);
		//后半段的长度
		int iLastLen = len - tmplen;
		memcpy((void *)mBufferPtr, (const void *)(buf + tmplen), iLastLen);
		*mEndIdxPtr = iLastLen;
		return 0;
	}
}

}	// namespace hnet
