
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MSG_QUEUE_
#define _W_MSG_QUEUE_

#include "wCore.h"
#include "wAssert.h"
#include "wNoncopyable.h"

#define MSG_QUEUE_RESERVE_LEN 8

/**
 *  消息传递的共享内存队列
 */
class wMsgQueue : private wNoncopyable
{
	public:
		void SetBuffer(char *vBuffer, int vBufferLen);
		/**
		 * 取出第一个消息，本函数只改变mBeginIdx
		 * @param  pBuffer    [out]
		 * @param  vBufferLen [一次最多取多少字节数据，大多数时刻返回值小于该值，取完一整条消息即可]
		 * @return            [< 0 出错，= 0 没消息，> 0 消息长度]
		 */
		int Pop(char *pBuffer, int vBufferLen);
		/**
		 * 放入第一个消息，本函数只改变mEndIdx
		 * @param  pBuffer    
		 * @param  vBufferLen 
		 * @return            = 0表示成功，< 0表示失败
		 */
		int Push(char *pBuffer, int vBufferLen);
		int IsEmpty() { return *mBeginIdxPtr == *mEndIdxPtr ? 1 : 0;}

	private:
		int *mBeginIdxPtr {NULL};	//前4位记录开始地址
		int *mEndIdxPtr {NULL};		//后4位开始地址
		char *mBufferPtr {NULL};	//实际数据地址
		int mQueueSize {0};			//实际数据长度
};

#endif
