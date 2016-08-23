
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMsgQueue.h"

void wMsgQueue::SetBuffer(char *vBuffer, int vBufferLen)
{
	char *pBuffer = vBuffer;
	mBeginIdxPtr = (int *)pBuffer;	//前4位开始地址
	pBuffer += sizeof(int);
	mEndIdxPtr = (int *)pBuffer;	//后4位开始地址
	pBuffer += sizeof(int);
	mBufferPtr = pBuffer;	//实际数据地址
	mQueueSize = vBufferLen - 2 * sizeof(int);	//实际数据长度
}

int wMsgQueue::Pop(char *pBuffer, int vBufferLen)
{
	if (pBuffer == NULL) return -1;

	//临时值，防止发送端同时操作产生同步问题
	int iBeginIdx = *mBeginIdxPtr;
	int iEndIdx = *mEndIdxPtr;

	if (iBeginIdx == iEndIdx) return 0;

	// 当前所有消息占据的长度
	int iAllMsgLen;
	if (iBeginIdx > iEndIdx)
	{
		iAllMsgLen = (int)mQueueSize - iBeginIdx + iEndIdx;
	}
	else
	{
		iAllMsgLen = iEndIdx - iBeginIdx;
	}
	
	//如果消息有问题，则直接忽略
	if (iAllMsgLen <= (int)sizeof(int))
	{
		*mBeginIdxPtr = iEndIdx;
		return -2;
	}

	//消息长度
	int iHeadMsgLen;
	//分段长度
	if (iBeginIdx > (int)mQueueSize - (int)sizeof(int)) 
	{
		int iLenCopyOnce = (int)mQueueSize - iBeginIdx;
		memcpy((void *)&iHeadMsgLen, (const void *)(mBufferPtr + iBeginIdx), iLenCopyOnce);
		memcpy((void *)(((char *)&iHeadMsgLen) + iLenCopyOnce), (const void *)mBufferPtr, (int)sizeof(int) - iLenCopyOnce);
	}
	else 
	{
		iHeadMsgLen = *(int *)(mBufferPtr + iBeginIdx);
	}

	// 消息长度判断
	if (iHeadMsgLen < 0 || iHeadMsgLen > iAllMsgLen - (int)sizeof(int)) return -3;
	
	// 如果接收长度过短
	if (iHeadMsgLen > vBufferLen) return -4;

	int iMsgEndIdx = iBeginIdx + (int)sizeof(int) + (int)iHeadMsgLen;
	//如果消息没有分段
	if (iMsgEndIdx <= (int)mQueueSize) 
	{
		memcpy((void *)pBuffer, (const void *)(mBufferPtr + iBeginIdx + (int)sizeof(int)), iHeadMsgLen);
		*mBeginIdxPtr = iMsgEndIdx;
		return iHeadMsgLen;
	}
	else 
	{
		//前半段的长度
		int iTmpLen = (int)mQueueSize - (int)sizeof(int) - iBeginIdx;
		if (iTmpLen <= 0) 
		{
			memcpy((void *)pBuffer, (const void *)(mBufferPtr - iTmpLen), iHeadMsgLen);
			*mBeginIdxPtr = iHeadMsgLen - iTmpLen;
			return iHeadMsgLen;
		}
		//拷贝前半段
		memcpy((void *)pBuffer, (const void *)(mBufferPtr + iBeginIdx + (int)sizeof(int)), iTmpLen);
		//后半段的长度
		int iLastLen = iHeadMsgLen - iTmpLen;
		//拷贝后半段
		memcpy((void *)(pBuffer + iTmpLen), (const void *)mBufferPtr, iLastLen);
		//调整起始位置
		*mBeginIdxPtr = iLastLen;
		return iHeadMsgLen;
	}
}

int wMsgQueue::Push(char *pBuffer, int vBufferLen)
{
	W_ASSERT(pBuffer != NULL || vBufferLen <= 0 || vBufferLen > mQueueSize - MSG_QUEUE_RESERVE_LEN, return -1);

	int iBeginIdx = *mBeginIdxPtr;
	int iEndIdx = *mEndIdxPtr;

	//剩余的空间长度
	int iLastLen;
	if (iBeginIdx == iEndIdx)
	{
		iLastLen = (int)mQueueSize;
	}
	else if (iBeginIdx > iEndIdx)
	{
		iLastLen = iBeginIdx - iEndIdx;
	}
	else
	{
		iLastLen = (int)mQueueSize - iEndIdx + iBeginIdx;
	}

	//减去保留长度
	iLastLen -= MSG_QUEUE_RESERVE_LEN;

	//如果剩余长度不够
	if (iLastLen < vBufferLen + (int)sizeof(int)) return -2;

	//消息长度判断
	if ((int)vBufferLen <= 0 || (int)vBufferLen > (int)mQueueSize - MSG_QUEUE_RESERVE_LEN) return -3;

	//拷贝长度，如果分段
	if (iEndIdx > (int)mQueueSize - (int)sizeof(int)) 
	{
		int iLenCopyOnce = (int)mQueueSize - iEndIdx;
		memcpy((void *)(mBufferPtr + iEndIdx), (const void *)&vBufferLen, iLenCopyOnce);
		memcpy((void *)mBufferPtr, (const void *)(((char *)&vBufferLen) + iLenCopyOnce), sizeof(int) - iLenCopyOnce);
	}
	// 如果不分段
	else
	{
		*(int *)(mBufferPtr + iEndIdx) = vBufferLen;
	}

	//确定当前的拷贝入口
	int iNowEndIdx = iEndIdx + (int)sizeof(int);
	if (iNowEndIdx >= (int)mQueueSize)
	{
		iNowEndIdx -= (int)mQueueSize;
	}

	int iMsgEndIdx = iNowEndIdx + vBufferLen;
	//不需要分段
	if (iMsgEndIdx <= (int)mQueueSize)
	{
		memcpy((void *)(mBufferPtr + iNowEndIdx), (const void *)pBuffer, vBufferLen);
		*mEndIdxPtr = iNowEndIdx + vBufferLen;
		return 0;
	}
	else
	{
		//前半段的长度
		int iTmpLen = (int)mQueueSize - iNowEndIdx;
		//拷贝消息前半段
		memcpy((void *)(mBufferPtr + iNowEndIdx), (const void *)pBuffer, iTmpLen);
		//后半段的长度
		int iLastLen = vBufferLen - iTmpLen;
		memcpy((void *)mBufferPtr, (const void *)(pBuffer + iTmpLen), iLastLen);
		*mEndIdxPtr = iLastLen;
		return 0;
	}
}
