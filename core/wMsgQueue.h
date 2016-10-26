
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MSG_QUEUE_
#define _W_MSG_QUEUE_

#include "wCore.h"
#include "wStatus.h"
#include "wNoncopyable.h"

namespace hnet {

const int kReserveLen = 8;

// 消息传递，常用于共享内存队列
class wMsgQueue : private wNoncopyable {
public:
	wMsgQueue() : mBeginIdxPtr(NULL), mEndIdxPtr(NULL), mBufferPtr(NULL), mQueueSize(0) { }

	void SetBuffer(char buf[], int len);

	// 取出第一个消息，本函数只改变mBeginIdx
	// 一次最多取多少字节数据，大多数时刻返回值小于该值，取完一整条消息即可
	// < 0 出错，= 0 没消息，> 0 消息长度
	int Pop(char *pBuffer, int vBufferLen);

	// 放入第一个消息，本函数只改变mEndIdx
	// = 0表示成功，< 0表示失败
	int Push(char *pBuffer, int vBufferLen);

	int IsEmpty() { return *mBeginIdxPtr == *mEndIdxPtr ? 1 : 0;}
private:
	// 前4位记录开始地址
	int *mBeginIdxPtr;
	// 后4位开始地址
	int *mEndIdxPtr;
	// 实际数据地址
	char *mBufferPtr;
	// 实际数据长度
	int mQueueSize;
};

}	// namespace hnet

#endif
