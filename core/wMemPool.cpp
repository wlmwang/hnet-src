
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <malloc.h>
#include <new>
#include "wMemPool.h"

namespace hnet {

wMemPool::~wMemPool() {
    for (size_t i = 0; i < mBlocks.size(); i++) {
        SAFE_DELETE_VEC(mBlocks[i]);
    }
}

char* wMemPool::AllocateFallback(size_t bytes) {
    if (bytes > kBlockSize / 4) {
        // 直接分配申请字节大小的新块内存返回
        char* result = AllocateNewBlock(bytes);
        return result;
    }

    // 最多浪费1023byte大小的当前内存节点中内存
    char* result = AllocateNewBlock(kBlockSize);
    if (result != NULL) {
        mAllocRemaining = kBlockSize;
        mAllocPtr = result + bytes;
        mAllocRemaining -= bytes;
    }
    return result;
}

char* wMemPool::AllocateAligned(size_t bytes) {
    const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
    assert((align & (align-1)) == 0);   // align = 2^n
    size_t current_mod = reinterpret_cast<uintptr_t>(mAllocPtr) & (align-1);   // 超出对齐字节数
    size_t slop = (current_mod == 0 ? 0 : align - current_mod);
    size_t needed = bytes + slop;   // 对齐时，实际申请字节数
    char* result;
    if (needed <= mAllocRemaining) {
        result = mAllocPtr + slop;
        mAllocPtr += needed;
        mAllocRemaining -= needed;
    } else {
        result = AllocateFallback(bytes);
    }
    assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);   // 对齐检查
    return result;
}

char* wMemPool::AllocateNewBlock(size_t block_bytes) {
    char* result;
    SAFE_NEW_VEC(block_bytes, char, result);
    if (result != NULL) {
        mBlocksMemory += block_bytes;
        mBlocks.push_back(result);
    }
    return result;
}

} // namespace hnet
