
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ATOMIC_H_
#define _W_ATOMIC_H_

#include <cstdint>
#include <atomic>
#include "wCore.h"

namespace hnet {

template <class T = void*>
class wAtomic {
private:
    std::atomic<T> mRep;

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
};

}   // namespace hnet

#endif