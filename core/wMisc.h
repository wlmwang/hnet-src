
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

#include "wCore.h"
#include "wCommand.h"
#include "wSignal.h"

inline const char* IP2Text(long ip)
{
	in_addr in;
	in.s_addr = ip;
	return inet_ntoa(in);
}

inline long Text2IP(const char* ipstr)
{
	return inet_addr(ipstr);
}

unsigned GetIpByIF(const char* pIfname);

inline unsigned long long GetTickCount()    //clock_t
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000;
}

inline int64_t GetTimeofday()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

inline void GetTimeofday(struct timeval* pVal, void*)
{
    if (pVal == NULL)
    {
        return;
    }
    gettimeofday(pVal, NULL);
}

inline int GetCwd(char *path, size_t size)
{
    if(getcwd(path, size) == NULL)
    {
        return -1;
    }
    return 0;
}

unsigned int HashString(const char* s);

//long -> string
inline string Itos(const long &i)
{
    string sTmp;
    stringstream sRet(sTmp);
    sRet << i;
    return sRet.str();
}

void Strlow(u_char *dst, u_char *src, size_t n);

//long -> char*
void itoa(unsigned long val, char *buf, unsigned radix);

u_char *Cpystrn(u_char *dst, u_char *src, size_t n);

vector<string> Split(string sStr, string sPattern, bool bRepeat = true);

int Gcd(int a, int b);
int Ngcd(int *arr, int n);

int InitDaemon(const char *filename, const char *prefix = NULL);

#endif
