
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_ATOMIC_POINTER_H_
#define _W_ATOMIC_POINTER_H_

#include <cstdint>
#include <atomic>
#include "wCore.h"

namespace hnet {

class wAtomicPointer {
private:
  std::atomic<void*> mRep;

public:
  wAtomicPointer() { }

  explicit wAtomicPointer(void* v) : mRep(v) { }

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

} // namespace hnet

#endif
