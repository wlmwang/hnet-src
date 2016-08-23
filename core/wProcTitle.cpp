
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wProcTitle.h"

wProcTitle::wProcTitle(int argc, const char *argv[])
{
	mArgc = argc;
	mOsArgv = (char **) argv;
	mOsArgvLast = (char *)argv[0];

    mOsEnv = environ;
    SaveArgv();
}

wProcTitle::~wProcTitle()
{
    for (int i = 0; i < mArgc; ++i)
    {
        SAFE_DELETE_VEC(mArgv[i]);    //delete []mArgv[i];
    }
    SAFE_DELETE_VEC(mArgv);   //delete []mArgv;
}

void wProcTitle::SaveArgv()
{
    size_t size = 0;
    
    //初始化argv内存空间
    mArgv = new char*[mArgc];
    for(int i = 0; i < mArgc; ++i)
    {
        size = strlen(mOsArgv[i]) + 1;  //包含\0结尾
        mArgv[i] = new char[size];
        memcpy(mArgv[i], mOsArgv[i], size);
    }
}

int wProcTitle::InitSetproctitle()
{
	u_char      *p;
	size_t size = 0;
	int   i;

	//environ字符串总长度
    for (i = 0; environ[i]; i++) 
    {
        size += strlen(environ[i]) + 1;		
    }

    p = new u_char[size];
    if (p == NULL) 
    {
        return -1;
    }

    //argv字符总长度
    for (i = 0; mOsArgv[i]; i++)
     {
        if (mOsArgvLast == mOsArgv[i]) 
        {
            mOsArgvLast = mOsArgv[i] + strlen(mOsArgv[i]) + 1;
        }
    }

    //移动**environ到堆上
    for (i = 0; environ[i]; i++) 
    {
        if (mOsArgvLast == environ[i]) 
        {
            size = strlen(environ[i]) + 1;
            mOsArgvLast = environ[i] + size;

            Cpystrn(p, (u_char *) environ[i], size);
            environ[i] = (char *) p;	//转化成char
            p += size;
        }
    }

    mOsArgvLast--;     //是原始argv和environ的总体大小。去除结尾一个NULL字符
    return 0;
}

void wProcTitle::Setproctitle(const char *title, const char *pretitle)
{
    u_char *p;
	u_char pre[512];   //标题，最长512字节

    mOsArgv[1] = NULL;

    if (pretitle == NULL)
    {
    	Cpystrn(pre, (u_char *) "disvr: ", sizeof(pre));
    }
    else
    {
    	Cpystrn(pre, (u_char *) pretitle, sizeof(pre));
    }

    p = Cpystrn((u_char *) mOsArgv[0], pre, mOsArgvLast - mOsArgv[0]);
    p = Cpystrn(p, (u_char *) title, mOsArgvLast - (char *) p);

    //在原始argv和environ的连续内存中，将修改了的进程名字之外的内存全部清零
    if(mOsArgvLast - (char *) p) 
    {
        memset(p, SETPROCTITLE_PAD, mOsArgvLast - (char *) p);
    }
}
