
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wThreadPool.h"

namespace hnet {

void wPosixEnv::Schedule(void (*function)(void*), void* arg) {
	pthread_mutex_lock(&mu_);

	if (!started_bgthread_) {   // 后台任务线程未开启
		started_bgthread_ = true;
		pthread_create(&bgthread_, NULL,  &wPosixEnv::BGThreadWrapper, this)
	}

	if (queue_.empty()) {
		pthread_cond_signal(&bgsignal_);	// 任务链表为空，唤醒任务消费线程
	}

	// 添加任务到队尾
	queue_.push_back(BGItem());
	queue_.back().function = function;
	queue_.back().arg = arg;

	pthread_mutex_unlock(&mu_);
}

// 后台任务线程主函数，是消费任务的守护线程
void wPosixEnv::BGThread() {
	while (true) {
    	// Wait until there is an item that is ready to run
    	pthread_mutex_lock(&mu_);
		while (queue_.empty()) {
			pthread_cond_wait(&bgsignal_, &mu_);
		}

		void (*function)(void*) = queue_.front().function;
		void* arg = queue_.front().arg;
		queue_.pop_front();

		pthread_mutex_unlock(&mu_);
		(*function)(arg);
	}
}

// 启动线程属性
namespace {
struct StartThreadState {
	void (*user_function)(void*);
	void* arg;
};
}	//namespace

// 线程启动包装器
static void* StartThreadWrapper(void* arg) {
	StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
	state->user_function(state->arg);
	delete state;
	return NULL;
}

// 启动线程
void wPosixEnv::StartThread(void (*function)(void* arg), void* arg) {
	pthread_t t;
	StartThreadState* state = new StartThreadState;
	state->user_function = function;
	state->arg = arg;
	pthread_create(&t, NULL,  &StartThreadWrapper, state);
}

}	// namespace hnet
