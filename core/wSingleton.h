
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SINGLETON_H_
#define _W_SINGLETON_H_

#include "wCore.h"

/**
 * 实现单例，该类是所有单例类的父类
 */
template <typename T>
class wSingletonFactory
{
	public:
		static T* Instance()
		{
			return new T();
		}
};

template <typename T, typename MANA = wSingletonFactory<T> > 
class wSingleton 
{
	private:
		static T *mSingletonPtr;

		//赋值操作符号，没有实现，禁用掉了
		const wSingleton &operator= (const wSingleton &);
	
	protected:
		wSingleton() {}
	
	public:
		virtual ~wSingleton() 
		{
			SAFE_DELETE(mSingletonPtr);
		}

		static T* Instance()
		{
			if (!mSingletonPtr)
			{
				mSingletonPtr = MANA::Instance();
			}
			return mSingletonPtr;
		}

		static T& GetMe(void)
		{
			return *Instance();
		}
};

template <typename T, typename MANA>
T* wSingleton<T,MANA>::mSingletonPtr = NULL;
#endif
