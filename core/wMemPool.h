
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MEM_POOL_H_
#define _W_MEM_POOL_H_

#include "wCore.h"
#include "wNoncopyable.h"

namespace hnet {

class wMemPool : private wNoncopyable {
public:
    wMemPool(): alloc_ptr_(NULL), alloc_bytes_remaining_(0), blocks_memory_(0) { }
    ~wMemPool();

    char* Allocate(size_t bytes);
    char* AllocateAligned(size_t bytes);

    size_t MemoryUsage() const {
        return blocks_memory_ + blocks_.capacity() * sizeof(char*);
    }

protected:
    char* AllocateFallback(size_t bytes);
    char* AllocateNewBlock(size_t block_bytes);

    char* alloc_ptr_;		// 下一次分配内存起始地址
    size_t alloc_bytes_remaining_;	// 当前内存池节点剩余字节大小
    size_t blocks_memory_;	// 内存池总字节数
    std::vector<char*> blocks_;	// 内存池链表
};

}   // namespace hnet

#endif
