
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <malloc.h>
#include <new>
#include "wMemPool.h"

namespace hnet {

wMemPool::~wMemPool() {
    for (size_t i = 0; i < blocks_.size(); i++) {
        delete[] blocks_[i];
    }
}


char* wMemPool::AllocateFallback(size_t bytes) {
    if (bytes > kBlockSize / 4) {
        char* result = AllocateNewBlock(bytes);		// 直接分配申请字节大小的新块内存返回
        return result;
    }

    // 最多浪费1023byte大小的当前内存节点中内存
    alloc_ptr_ = AllocateNewBlock(kBlockSize);
    alloc_bytes_remaining_ = kBlockSize;

    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
}

char* wMemPool::AllocateAligned(size_t bytes) {
    const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
    assert((align & (align-1)) == 0);   // Pointer size should be a power of 2  // align = 2^n
    size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align-1);   // 超出对齐字节数
    size_t slop = (current_mod == 0 ? 0 : align - current_mod);   // 补齐字节数，让其满足对齐
    size_t needed = bytes + slop;   // 对齐时，实际申请字节数
    char* result;
    if (needed <= alloc_bytes_remaining_) {
        result = alloc_ptr_ + slop;
        alloc_ptr_ += needed;
        alloc_bytes_remaining_ -= needed;
    } else {
        result = AllocateFallback(bytes);   // 分配新链表节点总是对齐的
    }
    assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);   // 对齐检查
    return result;
}

char* wMemPool::AllocateNewBlock(size_t block_bytes) {
    char* result = new char[block_bytes];
    blocks_memory_ += block_bytes;
    blocks_.push_back(result);
    return result;
}

} // namespace hnet
