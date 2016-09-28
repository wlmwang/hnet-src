
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_RANDOM_H_
#define _W_RANDOM_H_

#include "wCore.h"

namespace hnet {

class wRandom {
public:
    explicit wRandom(uint32_t s) : mSeed(s & 0x7fffffffu) {
        if (mSeed == 0 || mSeed == 2147483647L) {
            mSeed = 1;
        }
    }

    uint32_t Next() {
        static const uint32_t M = 2147483647L;   // 2^31-1
        static const uint64_t A = 16807;  // bits 14, 8, 7, 5, 2, 1, 0
        uint64_t product = mSeed * A;

        // Compute (product % M) using the fact that ((x << 31) % M) == x.
        mSeed = static_cast<uint32_t>((product >> 31) + (product & M));
        if (mSeed > M) {
            mSeed -= M;
        }
        return mSeed;
    }

    // 要求n>0
    // 返回[0..n-1]随机数
    uint32_t Uniform(int n) {
        return Next() % n; 
    }

    // 要求n>0
    // 测试1/n的概率
    bool OneIn(int n) {
        return (Next() % n) == 0; 
    }

    // 返回[0,2^max_log-1]的随机数
    uint32_t Skewed(int max_log) {
        return Uniform(1 << Uniform(max_log + 1));
    }
    
private:
    uint32_t mSeed;
};

}	// namespace hnet

#endif