
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MISC_H_
#define _W_MISC_H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/file.h>	//int flock(int fd,int operation);
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sstream>
#include <vector>
#include <cstdarg>
#include "wCore.h"
#include "wStatus.h"
#include "wCommand.h"
#include "wSignal.h"

namespace hnet {
namespace coding {

void EncodeFixed32(char* dst, uint32_t value);
void EncodeFixed64(char* dst, uint64_t value);

void PutFixed32(std::string* dst, uint32_t value);
void PutFixed64(std::string* dst, uint64_t value);

inline uint32_t DecodeFixed32(const char* ptr) {
    if (kLittleEndian) {
        uint32_t result;
        memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
        return result;
    } else {
        return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0])))
            | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8)
            | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16)
            | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
    }
}

inline uint64_t DecodeFixed64(const char* ptr) {
    if (kLittleEndian) {
        uint64_t result;
        memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
        return result;
    } else {
        uint64_t lo = DecodeFixed32(ptr);
        uint64_t hi = DecodeFixed32(ptr + 4);
        return (hi << 32) | lo;
    }
}

}   // namespace coding

namespace logging {

class wSlice;

// 将整型按字符方式追加到str字符串结尾
void AppendNumberTo(std::string* str, uint64_t num);

// 在str后面添加value字符，并将value不可见字符转化为16进制表示的字符
void AppendEscapedStringTo(std::string* str, const wSlice& value);

// 返回整型字符串
std::string NumberToString(uint64_t num);

// 返回不可见字符被转化为16进制表示的字符的value字符串
std::string EscapeString(const wSlice& value);

// 消费转化in字符串前缀的十进制数字符串，返回是否转化正确
bool ConsumeDecimalNumber(wSlice* in, uint64_t* val);

}   // namespace logging

namespace misc {

class wStatus;
// 创建守护进程
wStatus InitDaemon(const char *filename, const char *prefix = NULL);

// 哈希值 murmur hash类似算法
uint32_t Hash(const char* data, size_t n, uint32_t seed);

// 最大公约数
uint64_t Ngcd(uint64_t arr[], size_t n);

// 网卡获取地址
unsigned GetIpByIF(const char* ifname);

// 转化成小写
void Strlow(u_char *dst, u_char *src, size_t n);

// 复制字符串
u_char *Cpystrn(u_char *dst, u_char *src, size_t n);

inline const char* IP2Text(u_long ip) {
    in_addr in;
    in.s_addr = ip;
    return inet_ntoa(in);
}

inline uint32_t Text2IP(const char* cp) {
    return static_cast<uint32_t>(inet_addr(cp));    // typedef in_addr_t u_long
}

inline void GetTimeofday(struct timeval* pVal, void*) {
    pVal != NULL && gettimeofday(pVal, NULL);
}

inline int64_t GetTimeofday() {
    struct timeval tv;
    GetTimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

inline uint8_t AlignMent() {
    return sizeof(unsigned long);
}

inline uint32_t Align(uint32_t d, uint32_t a) {
    return (((d) + (a - 1)) & ~(a - 1));
}

inline u_char* AlignPtr(char* p, char* a) {
    return (u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1));
}

template <typename T = uint64_t>
inline void Swap(T a, T b) {
    T tmp = a; a = b; b = tmp;
}

template <typename T>
inline T* SafeNew() {
    T* ptr = NULL;
    try {
        ptr = new T();
    } catch (...) {
        ptr = NULL;
    }
    return ptr;
}

template <typename T>
inline T* SafeNewVec(size_t n) {
    T* ptr = NULL;
    try {
        ptr = new T[n];
    } catch (...) {
        ptr = NULL;
    }
    return ptr;
}

template <typename T>
inline void SafeDelete(T* ptr) {
    if (ptr) {
        delete ptr;
        ptr = NULL;
    }
}

template <typename T>
inline void SafeDeleteVec(T* ptr) {
    if (ptr) {
        delete[] ptr;
        ptr = NULL;
    }
}

}   // namespace misc
}   // namespace hnet

#endif
