
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_POSIX_ENV_H_
#define _W_POSIX_ENV_H_

#include <algorithm>
#include <deque>

#include "wCore.h"
#include "wMutex.h"
#include "wNoncopyable.h"
#include "wThread.h"

namespace hnet {

class wPosixEnv : private wNoncopyable {
public:
	wPosixEnv() : started_bgthread_(false) {
		pthread_mutex_init(&mu_, NULL)
		pthread_cond_init(&bgsignal_, NULL);
	}

	~wPosixEnv();

	// 添加任务到后台任务消费线程中
	virtual void Schedule(void (*function)(void*), void* arg);
	virtual void StartThread(void (*function)(void* arg), void* arg);

private:
	// 后台任务线程主函数
	void BGThread();

	// 后台任务线程入口函数，为主函数BGThread的包装器
	static void* BGThreadWrapper(void* arg) {
		reinterpret_cast<wPosixEnv*>(arg)->BGThread();
		return NULL;
	}

	pthread_mutex_t mu_;
	pthread_cond_t bgsignal_;	// 消费线程条件变量
	pthread_t bgthread_;     // 后台任务消费线程id
	bool started_bgthread_;	// 是否已开启了后台任务消费线程

	// 线程队列节点结构体
	struct BGItem {
		void* arg; 
		void (*function)(void*);
	};
	typedef std::deque<BGItem> BGQueue;
	
	BGQueue queue_; // 任务双端链表
};

}	// namespace hnet

#endif
