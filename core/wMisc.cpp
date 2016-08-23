
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wMisc.h"

unsigned GetIpByIF(const char* pIfname)
{
    int iFD, iIntrface;
    struct ifreq buf[64];
    struct ifconf ifc = {0, {0}};
    unsigned ip = 0; 

    memset(buf, 0, sizeof(buf));
    if ((iFD = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = (caddr_t)buf;
        if (!ioctl(iFD, SIOCGIFCONF, (char*)&ifc))
        {
            iIntrface = ifc.ifc_len / sizeof(struct ifreq); 
            while (iIntrface-- > 0)
            {
                if (strcmp(buf[iIntrface].ifr_name, pIfname) == 0)
                {
                    if (!(ioctl(iFD, SIOCGIFADDR, (char *)&buf[iIntrface])))
                    {
                        ip = (unsigned)((struct sockaddr_in *)(&buf[iIntrface].ifr_addr))->sin_addr.s_addr;
                    }
                    break;  
                }
            }
        }
        close(iFD);
    }
    return ip;
}

void Strlow(u_char *dst, u_char *src, size_t n)
{
    while (n) 
    {
        *dst = tolower(*src);
        dst++;
        src++;
        n--;
    }
}

unsigned int HashString(const char* s)
{
	unsigned int hash = 5381;
	while (*s)
	{
		hash += (hash << 5) + (*s ++);
	}
	return hash & 0x7FFFFFFF;
}

void itoa(unsigned long val, char *buf, unsigned radix) 
{
	char *p; /* pointer to traverse string */ 
	char *firstdig; /* pointer to first digit */ 
	char temp; /* temp char */ 
	unsigned digval; /* value of digit */ 

	p = buf; 
	firstdig = p; /* save pointer to first digit */ 

	do { 
		digval = (unsigned) (val % radix); 
		val /= radix; /* get next digit */ 

		/* convert to ascii and store */ 
		if (digval > 9) 
			*p++ = (char ) (digval - 10 + 'a'); /* a letter */ 
		else 
			*p++ = (char ) (digval + '0'); /* a digit */ 
	} while (val > 0); 

	/* We now have the digit of the number in the buffer, but in reverse 
	   order. Thus we reverse them now. */ 

	*p-- = '\0'; /* terminate string; p points to last digit */ 

	do { 
		temp = *p; 
		*p = *firstdig; 
		*firstdig = temp; /* swap *p and *firstdig */ 
		--p; 
		++firstdig; /* advance to next two digits */ 
	} while (firstdig < p); /* repeat until halfway */ 
}

vector<string> Split(string sStr, string sPattern, bool bRepeat)  
{  
    string::size_type iPos, iNextPos;
    vector<string> vResult;
    sStr += sPattern;  
    int iSize = sStr.size();
  
    for (int i = 0; i < iSize; i++)  
    {  
        iPos = iNextPos = sStr.find(sPattern, i);
        if ((int)iPos < iSize)
        {
            string s = sStr.substr(i, iPos - i);
            vResult.push_back(s);
            i = iPos + sPattern.size() - 1;
        }
    }
    return vResult;
}

u_char *Cpystrn(u_char *dst, u_char *src, size_t n)
{
    if (n == 0) 
    {
        return dst;
    }

    while (--n) 
    {
        *dst = *src;
        if (*dst == '\0') 
        {
            return dst;
        }
        dst++;
        src++;
    }

    *dst = '\0';
    return dst;
}

int Gcd(int a, int b)
{
	if (a < b)
	{
		int tmp = a;
		a = b;
		b = tmp;
	}

	if (b == 0) return a;
	return Gcd(b, a % b);
}

int Ngcd(int *arr, int n)
{
	if (n == 1)  return *arr;
	return Gcd(arr[n-1], Ngcd(arr, n-1));
}

int InitDaemon(const char *filename, const char *prefix)
{
	//打开需要锁定的文件
	if (filename == NULL)
	{
		filename = LOCK_PATH;
	}

	//切换当前目录
	char dir_path[256] = {0};
	if (prefix == NULL)
	{
		if (GetCwd(dir_path, sizeof(dir_path)) == -1)
		{
			cout << "[system] getcwd failed: " << strerror(errno) << endl;
			exit(0);
		}
	}
	else
	{
		memcpy(dir_path, prefix, strlen(prefix) + 1);
	}

	//切换工作目录
	if (chdir(dir_path) == -1) 
	{
		cout << "[system] Can not change run dir to "<< dir_path << " , init daemon failed: " << strerror(errno) << endl;
		return -1;
	}
	umask(0);

	int lock_fd = open(filename, O_RDWR|O_CREAT, 0640);
	if (lock_fd < 0) 
	{
		cout << "[system] Open lock file failed when init daemon" << endl;
		return -1;
	}
	//独占式锁定文件，防止有相同程序的进程已经启动
	if (flock(lock_fd, LOCK_EX | LOCK_NB) < 0) 
	{
		cout << "[system] Lock file failed, server is already running" << endl;
		return -1;
	}

    /**
     * 设置进程的有效uid
     * 若是以root身份运行，则将worker进程降级, 默认是nobody
     */
	/*
    if (geteuid() == 0) 
	{
        if (setgid(GROUP) == -1) 
		{
			LOG_ERROR(ELOG_KEY, "[system] setgid(%d) failed: %s", GROUP, strerror(mErr));
            exit(2);
        }

        //附加组ID
        if (initgroups(USER, GROUP) == -1) 
		{
			LOG_ERROR(ELOG_KEY, "[system] initgroups(%s, %d) failed: %s", USER, GROUP, strerror(mErr));
        }

        //用户ID
        if (setuid(USER) == -1) 
		{
			LOG_ERROR(ELOG_KEY, "[system] setuid(%d) failed: %s", USER, strerror(mErr));            
			exit(2);
        }
    }
	*/

	//第一次fork
	if (fork() != 0) exit(0);

	setsid();

	//忽略以下信号
	wSignal stSig(SIG_IGN);
	stSig.AddSigno(SIGINT);
	stSig.AddSigno(SIGHUP);
	stSig.AddSigno(SIGQUIT);
	stSig.AddSigno(SIGTERM);
	stSig.AddSigno(SIGCHLD);
	stSig.AddSigno(SIGPIPE);
	stSig.AddSigno(SIGTTIN);
	stSig.AddSigno(SIGTTOU);

	//再次fork
	if (fork() != 0) exit(0);
	
	//unlink(filename);
	return 0;
}
