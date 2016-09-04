
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_CRC32C_H_
#define _W_CRC32C_H_

#include "wCore.h"

namespace hnet {
namespace crc32c {

// 掩码
static const uint32_t kMaskDelta = 0xa282ead8ul;

// 返回concat(A, data[0,n-1])的crc32c值，其中字符串A的crc32c值为init_crc
extern uint32_t Extend(uint32_t init_crc, const char* data, size_t n);

// 返回data[0,n-1]的crc32c值
inline uint32_t Value(const char* data, size_t n) {
    return Extend(0, data, n);
}

// 计算包含嵌入crc字符的crc码会发生问题，建议在存储时做掩码旋转处理
inline uint32_t Mask(uint32_t crc) {
    return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

// 还原crc码
inline uint32_t Unmask(uint32_t masked_crc) {
    uint32_t rot = masked_crc - kMaskDelta;
    return ((rot >> 17) | (rot << 15));
}

}  // namespace crc32c
}  // namespace hnet

#endif