
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

class wAtomic {
private:
    std::atomic<void*> mRep;

public:
    wAtomic() { }

    explicit wAtomic(void* v) : mRep(v) { }

    inline void* Acquire_Load() const {
        return mRep.load(std::memory_order_acquire);
    }

    inline void Release_Store(void* v) {
        mRep.store(v, std::memory_order_release);
    }

    inline void* NoBarrier_Load() const {
        return mRep.load(std::memory_order_relaxed);
    }

    inline void NoBarrier_Store(void* v) {
        mRep.store(v, std::memory_order_relaxed);
    }
};

}   // namespace hnet

#endif