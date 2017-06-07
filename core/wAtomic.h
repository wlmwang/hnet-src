
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ATOMIC_H_
#define _W_ATOMIC_H_

#include <cstdint>
#include <atomic>
#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

template <class T>
class wAtomic : private wNoncopyable {
public:
    wAtomic() { }

    explicit wAtomic(T v) : mRep(v) { }

    inline T AcquireLoad() const {
        return mRep.load(std::memory_order_acquire);
    }

    inline void ReleaseStore(T v) {
        mRep.store(v, std::memory_order_release);
    }

    inline T NoBarrierLoad() const {
        return mRep.load(std::memory_order_relaxed);
    }

    inline void NoBarrierStore(T v) {
        mRep.store(v, std::memory_order_relaxed);
    }

    // 更改为v并返回原来的值
    inline T Exchange(T v) {
        return mRep.exchange(v, std::memory_order_acq_rel);
    }

    // 比较被封装的值(weak)与参数expected所指定的值的物理内容是否相等，如果相等，则用v替换原子对象的旧值；如果不相等，则用原子对象的旧值替换expected
    // 
    // 调用该函数之后，如果被该原子对象封装的值与参数 expected 所指定的值不相等，expected 中的内容就是原子对象的旧值。
    // 整个操作是原子的，当前线程读取和修改该原子对象时，别的线程不能对读取和修改该原子对象。
    // 
    // 与compare_exchange_strong不同, weak版本的compare-and-exchange操作允许spuriously 返回 false。
    // 即原子对象所封装的值与参数 expected 的物理内容相同，但却仍然返回 false，因为把desired赋值给对象会失败（特别是对于没有compare/exchange指令的CPU），失败的情况下不会更新std::atomic对象的值。
    // 这在使用循环操作的算法下是可以接受的，并且在一些平台下weak版本的性能更好。
    inline bool CompareExchangeWeak(T expected, T desired) {
        return mRep.compare_exchange_weak(expected, desired, std::memory_order_seq_cst);
    }

    // 与weak版本的差别在于，如果原子对象所封装的值与参数 expected 的物理内容相同，比较操作一定会为 true
    inline bool CompareExchangeStrong(T expected, T desired) {
        return mRep.compare_exchange_strong(expected, desired, std::memory_order_seq_cst);
    }

protected:
    std::atomic<T> mRep;
};

}   // namespace hnet

#endif