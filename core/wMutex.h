
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MUTEX_H_
#define _W_MUTEX_H_

#include <pthread.h>

#include "wCore.h"
#include "wLog.h"
#include "wNoncopyable.h"

class wCond;
class wMutex : private wNoncopyable
{
	friend class wCond;
	public:
		/**
		 *  PTHREAD_MUTEX_FAST_NP: 普通锁，同一线程可重复加锁，解锁一次释放锁。不提供死锁检测，尝试重新锁定互斥锁会导致死锁
		 *  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP：同一线程可重复加锁，解锁同样次数才可释放锁
		 *  PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP：同一线程不能重复加锁，加上的锁只能由本线程解锁
		 *  
		 *  PTHREAD_PROCESS_PRIVATE：单进程锁，默认设置
		 *  PTHREAD_PROCESS_SHARED：进程间，共享锁（锁变量需进程间共享。如在共享内存中）
		 */
		wMutex(int kind = PTHREAD_MUTEX_FAST_NP, int pshared = PTHREAD_PROCESS_PRIVATE) 
		{
			pthread_mutexattr_init(&mAttr);
			pthread_mutexattr_settype(&mAttr, kind);
			pthread_mutexattr_setpshared(&mAttr, pshared);
			if (pthread_mutex_init(&mMutex, &mAttr) < 0)
			{
				mErr = errno;
				LOG_ERROR(ELOG_KEY, "[system] pthread_mutex_init failed:%s", strerror(mErr));
				exit(2);
			}
		}
		
		~wMutex()
		{
			pthread_mutexattr_destroy(&mAttr);
			pthread_mutex_destroy(&mMutex);
		}
		
		/**
		 *  阻塞获取锁
		 *  0		成功
		 *  EINVAL	锁不合法，mMutex 未被初始化
		 *  EDEADLK	重复加锁错误
		 * 	...
		 */
		inline int Lock()
		{
			return pthread_mutex_lock(&mMutex);
		}
		
		/**
		 *  非阻塞获取锁
		 *  0		成功
		 *  EBUSY	锁正在使用
		 *  EINVAL	锁不合法，mMutex 未被初始化
		 *  EAGAIN	Mutex的lock count(锁数量)已经超过 递归索的最大值，无法再获得该mutex锁
		 *  EDEADLK	重复加锁错误
		 */
		inline int TryLock()
		{
			return pthread_mutex_trylock(&mMutex);
		}
		
		/**
		 *  释放锁
		 *  0		成功
		 *  EPERM	当前线程不是该 mMutex 锁的拥有者
		 */
		inline int Unlock()
		{
			return pthread_mutex_unlock(&mMutex);
		}
		
	protected:
		pthread_mutex_t mMutex;
		pthread_mutexattr_t mAttr;
		int mErr;
};

//读写锁
class RWLock : private wNoncopyable
{
	public:
		RWLock(int pshared = PTHREAD_PROCESS_PRIVATE) 
		{
			pthread_rwlockattr_init(&mAttr);
			pthread_rwlockattr_setpshared(&mAttr, pshared);
			pthread_rwlock_init(&mRWlock, &mAttr);
		}
		
		~RWLock()
		{
			pthread_rwlockattr_destroy(&mAttr);
			pthread_rwlock_destroy(&mRWlock);
		}
		
		/**
		 *  阻塞获取读锁
		 *  0		成功
		 *  EINVAL	锁不合法，mRWlock 未被初始化
		 *  EAGAIN	Mutex的lock count(锁数量)已经超过 递归索的最大值，无法再获得该mutex锁
		 *  EDEADLK	重复加锁错误
		 * 	...
		 */
		inline int LockRD()
		{
			if(pthread_rwlock_rdlock(&mRWlock) == -1)	//可能被锁打断
			{
				mErrno = errno;
				return -1;
			}
			return 0;
		}

		/**
		 *  阻塞获取写锁
		 *  0		成功
		 *  EINVAL	锁不合法，mRWlock 未被初始化
		 *  EAGAIN	Mutex的lock count(锁数量)已经超过 递归索的最大值，无法再获得该mutex锁
		 *  EDEADLK	重复加锁错误
		 * 	...
		 */
		inline int LockWR()
		{
			if(pthread_rwlock_wrlock(&mRWlock) == -1)	//可能被锁打断
			{
				mErrno = errno;
				return -1;
			}
			return 0;
		}
		
		/**
		 *  非阻塞获取读锁
		 *  0		成功
		 *  EBUSY	锁正在使用
		 *  EINVAL	锁不合法，mRWlock 未被初始化
		 *  EAGAIN	Mutex的lock count(锁数量)已经超过 递归索的最大值，无法再获得该mutex锁
		 *  EDEADLK	重复加锁错误
		 */
		inline int TryLockRD()
		{
			return pthread_rwlock_tryrdlock(&mRWlock);
		}
		
		/**
		 *  非阻塞获取写锁
		 *  0		成功
		 *  EBUSY	锁正在使用
		 *  EINVAL	锁不合法，mRWlock 未被初始化
		 *  EAGAIN	Mutex的lock count(锁数量)已经超过 递归索的最大值，无法再获得该mutex锁
		 *  EDEADLK	重复加锁错误
		 */
		inline int TryLockWR()
		{
			return pthread_rwlock_trywrlock(&mRWlock);
		}
		
		/**
		 *  释放锁
		 *  0		成功
		 *  EPERM	当前线程不是该 mRWlock 锁的拥有者
		 */
		inline int Unlock()
		{
			return pthread_rwlock_unlock(&mRWlock);
		}
		
	protected:
		pthread_rwlock_t mRWlock;
		pthread_rwlockattr_t mAttr;
		int mErrno;
};

class wCond : private wNoncopyable
{
	public:
		/**
		 *  PTHREAD_PROCESS_PRIVATE：单进程条件变量，默认设置
		 *  PTHREAD_PROCESS_SHARED：进程间，共享条件变量（条件变量需进程间共享。如在共享内存中） 
		 */
		wCond(int pshared = PTHREAD_PROCESS_PRIVATE)
		{
			pthread_condattr_init(&mAttr);
			pthread_condattr_setpshared(&mAttr, pshared);
			pthread_cond_init(&mCond, &mAttr);
		}
		
		~wCond()
		{
			pthread_condattr_destroy(&mAttr);
			pthread_cond_destroy(&mCond);
		}
		
		int Broadcast()
		{
			return pthread_cond_broadcast(&mCond);
		}
		
		/**
		 *  唤醒等待中的线程
		 *  使用pthread_cond_signal不会有"惊群现象"产生，他最多只给一个线程发信号
		 */
		int Signal()
		{
			return pthread_cond_signal(&mCond);
		}

		/**
		 * 等待特定的条件变量满足
		 * @param stMutex 需要等待的互斥体
		 */
		int Wait(wMutex &stMutex)
		{
			return pthread_cond_wait(&mCond, &stMutex.mMutex);
		}
		
		/**
		 * 带超时的等待条件
		 * @param  stMutex 需要等待的互斥体
		 * @param  tsptr   超时时间
		 * @return         
		 */
		int TimedWait(wMutex &stMutex, struct timespec *tsptr)
		{
			return pthread_cond_timedwait(&mCond, &stMutex.mMutex, tsptr);
		}
		
	private:
		pthread_cond_t mCond;		//系统条件变量
		pthread_condattr_t mAttr;
};

#endif
