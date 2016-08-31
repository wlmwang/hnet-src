
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_MISC_H_
#define _W_MISC_H_

#include <sstream>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/file.h>	//int flock(int fd,int operation);

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "wCore.h"
#include "wCommand.h"
#include "wSignal.h"

namespace hnet
{
    namespace coding {
        const bool kLittleEndian = true;    // 小端
        
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
        // 将整型按字符方式追加到str字符串结尾
        void AppendNumberTo(std::string* str, uint64_t num);

        // 在str后面添加value字符，并将value不可见字符转化为16进制表示的字符
        void AppendEscapedStringTo(std::string* str, const std::string& value);

        // 返回整型字符串
        std::string NumberToString(uint64_t num);

        // 返回不可见字符被转化为16进制表示的字符的value字符串
        std::string EscapeString(const std::string& value);

        // 消费转化in字符串前缀的十进制数字符串，返回是否转化正确
        bool ConsumeDecimalNumber(std::string* in, uint64_t* val);

    }

    namespace misc {

        // 哈希值 murmur hash类似算法
        uint32_t Hash(const char* data, size_t n, uint32_t seed);

        // 转化成小写
        void Strlow(u_char *dst, u_char *src, size_t n);

        // 复制字符串
        u_char *Cpystrn(u_char *dst, u_char *src, size_t n);
        
        // 最大公约数
        uint64_t Ngcd(uint64_t arr[], size_t n);

        // 创建守护进程
        uint64_t InitDaemon(const char *filename, const char *prefix = NULL);
        
        // 网卡获取地址
        unsigned GetIpByIF(const char* pIfname);
        
        template <typename T = uint64_t>
        void Swap(T a, T b) {
            a = a+b;
            b = a-b;
            a = a-b;
        }

        inline const char* IP2Text(long ip) {
            in_addr in;
            in.s_addr = ip;
            return inet_ntoa(in);
        }

        inline long Text2IP(const char* ipstr) {
            return inet_addr(ipstr);
        }

        inline unsigned long long GetTickCount() {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000;
        }

        inline int64_t GetTimeofday() {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
        }

        inline void GetTimeofday(struct timeval* pVal, void*) {
            pVal != NULL && gettimeofday(pVal, NULL);
        }

        inline int GetCwd(char *path, size_t size) {
            if(getcwd(path, size) == NULL) return -1;
            return 0;
        }
    }

}

#endif
