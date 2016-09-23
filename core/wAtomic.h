
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

    inline void* AcquireLoad() const {
        return mRep.load(std::memory_order_acquire);
    }

    inline void ReleaseStore(void* v) {
        mRep.store(v, std::memory_order_release);
    }

    inline void* NoBarrierLoad() const {
        return mRep.load(std::memory_order_relaxed);
    }

    inline void NoBarrierStore(void* v) {
        mRep.store(v, std::memory_order_relaxed);
    }
};

}   // namespace hnet

#endif