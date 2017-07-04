
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_THREAD_H_
#define _W_THREAD_H_

#include <pthread.h>
#include <cstdarg>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

class wMutex;
class wCond;

class wThread : private wNoncopyable {
public:
    // 线程分离状态决定一个线程以什么样的方式来终止自己
    // 
    // 结合线程：能够被其他线程收回其资源和杀死，在被其他线程回收之前，它的存储器资源（如栈）是不释放的。
    // 只有当pthread_join()返回时，创建的线程才算终止，释放自己占用的系统资源
    // 
    // 分离线程：不能被其他线程回收或杀死的，它的存储器资源在它终止时由系统自动释放
    wThread(bool join = true);
    virtual ~wThread();

    // 启动线程：即创建线程
    int StartThread();

    // 等待线程结束，并回收资源
    // 提前是，将线程设置成joinable可结合的状态启动
    int JoinThread();

    inline bool Joinable() { return mJoinable; }

    // 线程实际执行的用户函数
    virtual int RunThread() = 0;
    virtual int PrepareThread() { return 0;}

protected:
    static void* ThreadWrapper(void *argv);

protected:
    pthread_attr_t mAttr;
    pthread_t mPthreadId;
    
    bool mJoinable;
    bool mAlive;
        
    wMutex *mMutex;
    wCond *mCond;
};

}	// namespace hnet

#endif
