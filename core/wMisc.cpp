
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include "wMisc.h"
#include "wAtomic.h"
#include "wLogger.h"

namespace hnet {
namespace coding {

void EncodeFixed8(char* buf, uint8_t value) {
    if (kLittleEndian) {
        memcpy(buf, &value, sizeof(value));
    } else {
        buf[0] = value & 0xff;
    }
}

void EncodeFixed16(char* buf, uint16_t value) {
    if (kLittleEndian) {
        memcpy(buf, &value, sizeof(value));
    } else {
        buf[0] = value & 0xff;
        buf[1] = (value >> 8) & 0xff;
    }
}

void EncodeFixed32(char* buf, uint32_t value) {
    if (kLittleEndian) {
        memcpy(buf, &value, sizeof(value));
    } else {
        buf[0] = value & 0xff;
        buf[1] = (value >> 8) & 0xff;
        buf[2] = (value >> 16) & 0xff;
        buf[3] = (value >> 24) & 0xff;
    }
}

void EncodeFixed64(char* buf, uint64_t value) {
    if (kLittleEndian) {
        memcpy(buf, &value, sizeof(value));
    } else {
        buf[0] = value & 0xff;
        buf[1] = (value >> 8) & 0xff;
        buf[2] = (value >> 16) & 0xff;
        buf[3] = (value >> 24) & 0xff;
        buf[4] = (value >> 32) & 0xff;
        buf[5] = (value >> 40) & 0xff;
        buf[6] = (value >> 48) & 0xff;
        buf[7] = (value >> 56) & 0xff;
    }
}

void PutFixed32(std::string* dst, uint32_t value) {
    char buf[sizeof(value)];
    EncodeFixed32(buf, value);
    dst->append(buf, sizeof(buf));
}

void PutFixed64(std::string* dst, uint64_t value) {
    char buf[sizeof(value)];
    EncodeFixed64(buf, value);
    dst->append(buf, sizeof(buf));
}

}	// namespace coding

namespace logging {

void AppendNumberTo(std::string* str, uint64_t num) {
    char buf[30];
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long) num);
    str->append(buf);
}

void AppendEscapedStringTo(std::string* str, const wSlice& value) {
    for (size_t i = 0; i < value.size(); i++) {
        char c = value[i];
        if (c >= ' ' && c <= '~') {		// 可见字符范围
            str->push_back(c);
        } else {
            char buf[10];
            // 转成\x[0-9]{2} 16进制输出，前缀补0
            snprintf(buf, sizeof(buf), "\\x%02x", static_cast<unsigned int>(c) & 0xff);
            str->append(buf);
        }
    }
}

std::string NumberToString(uint64_t num) {
    std::string r;
    AppendNumberTo(&r, num);
    return r;
}

std::string EscapeString(const wSlice& value) {
    std::string r;
    AppendEscapedStringTo(&r, value);
    return r;
}

bool DecimalStringToNumber(const std::string& in, uint64_t* val, uint8_t *width) {
    uint64_t v = 0;
    uint8_t digits = 0;
    while (!in.empty()) {
        char c = in[digits];
        if (c >= '0' && c <= '9') {
            ++digits;
            const int delta = (c - '0');
            static const uint64_t kMaxUint64 = ~static_cast<uint64_t>(0);
            if (v > kMaxUint64/10 || (v == kMaxUint64/10 && static_cast<uint64_t>(delta) > kMaxUint64%10)) {
            	// 转化uint64溢出
                return false;
            }
            v = (v * 10) + delta;
        } else {
            break;
        }
    }
    *val = v;
    (width != NULL) && (*width = digits);
    return (digits > 0);
}

}	// namespace logging

namespace misc {

static inline void FallThroughIntended() { }

uint32_t Hash(const char* data, size_t n, uint32_t seed) {
    const uint32_t m = 0xc6a4a793;
    const uint32_t r = 24;
    const char* limit = data + n;
    uint32_t h = seed ^ (n * m);

    // Pick up four bytes at a time
    while (data + 4 <= limit) {
        uint32_t w = coding::DecodeFixed32(data);
        data += 4;
        h += w;
        h *= m;
        h ^= (h >> 16);
    }

    switch (limit - data) {
        case 3:
        h += static_cast<unsigned char>(data[2]) << 16;
        FallThroughIntended();
        
        case 2:
        h += static_cast<unsigned char>(data[1]) << 8;
        FallThroughIntended();

        case 1:
        h += static_cast<unsigned char>(data[0]);
        h *= m;
        h ^= (h >> r);
        break;
    }
    return h;
}

char *Cpystrn(char *dst, const char *src, size_t n) {
    if (n == 0) return dst;

    while (--n) {
        *dst = *src;
        if (*dst == '\0') {
            return dst;
        }
        dst++;
        src++;
    }
    *dst = '\0';
    return dst;
}

void Strlow(char *dst, const char *src, size_t n) {
    do {
        *dst = tolower(*src);
        dst++;
        src++;
    } while (--n);
}

void Strupper(char *dst, const char *src, size_t n) {
    do {
        *dst = toupper(*src);
        dst++;
        src++;
    } while (--n);
}

int32_t Strcmp(const std::string& str1, const std::string& str2, size_t n) {
    return str1.compare(0, n, str2, 0, n);
}

int32_t Strpos(const std::string& haystack, const std::string& needle) {
    std::string::size_type pos = haystack.find(needle);
    if (pos != std::string::npos) {
        return pos;
    }
    return -1;
}

std::vector<std::string> SplitString(const std::string& src, const std::string& delim) {
    std::vector<std::string> dst;
    std::string::size_type pos1 = 0, pos2 = src.find(delim);
    while (std::string::npos != pos2) {
        dst.push_back(src.substr(pos1, pos2-pos1));

        pos1 = pos2 + delim.size();
        pos2 = src.find(delim, pos1);
    }
    if (pos1 != src.length()) {
        dst.push_back(src.substr(pos1));
    }
    return dst;
}

static uint64_t Gcd(uint64_t a, uint64_t b) {
    if (a < b) std::swap(a, b);
    if (b == 0) return a;
    return Gcd(b, a % b);
}

uint64_t Ngcd(uint64_t *arr, size_t n) {
    if (n <= 1)  return arr[n-1];
    return Gcd(arr[n-1], Ngcd(arr, n-1));
}

int GetIpList(std::vector<unsigned int>& iplist) {
    unsigned int ip = 0;
    ssize_t fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        struct ifconf ifc = {0, {0}};
        struct ifreq buf[64];
        memset(buf, 0, sizeof(buf));
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = reinterpret_cast<caddr_t>(buf);
        if (!ioctl(fd, SIOCGIFCONF, (char*)&ifc)) {
            size_t interface = ifc.ifc_len / sizeof(struct ifreq); 
            while (interface-- > 0) {
                if (!ioctl(fd, SIOCGIFADDR, reinterpret_cast<char*>(&buf[interface]))) {
                    ip = reinterpret_cast<unsigned>((reinterpret_cast<struct sockaddr_in*>(&buf[interface].ifr_addr))->sin_addr.s_addr);
                    iplist.push_back(ip);
                }
            }
        }
        close(fd);
        return 0;
    }
    return -1;
}

unsigned int GetIpByIF(const char* ifname) {
    unsigned int ip = 0;
    ssize_t fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        struct ifconf ifc = {0, {0}};
        struct ifreq buf[64];
        memset(buf, 0, sizeof(buf));
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = reinterpret_cast<caddr_t>(buf);
        if (!ioctl(fd, SIOCGIFCONF, (char*)&ifc)) {
            size_t interface = ifc.ifc_len / sizeof(struct ifreq); 
            while (interface-- > 0) {
                if (strcmp(buf[interface].ifr_name, ifname) == 0) {
                    if (!ioctl(fd, SIOCGIFADDR, reinterpret_cast<char*>(&buf[interface]))) {
                        ip = reinterpret_cast<unsigned>((reinterpret_cast<struct sockaddr_in*>(&buf[interface].ifr_addr))->sin_addr.s_addr);
                    }
                    break;  
                }
            }
        }
        close(fd);
    }
    return ip;
}

int FastUnixSec2Tm(time_t unix_sec, struct tm* tm, int time_zone) {
    static const int kHoursInDay = 24;
    static const int kMinutesInHour = 60;
    static const int kDaysFromUnixTime = 2472632;
    static const int kDaysFromYear = 153;
    static const int kMagicUnkonwnFirst = 146097;
    static const int kMagicUnkonwnSec = 1461;
    tm->tm_sec =  unix_sec % kMinutesInHour;
    int i = (unix_sec/kMinutesInHour);
    tm->tm_min = i % kMinutesInHour;
    i /= kMinutesInHour;
    tm->tm_hour = (i + time_zone) % kHoursInDay;
    tm->tm_mday = (i + time_zone) / kHoursInDay;
    int a = tm->tm_mday + kDaysFromUnixTime;
    int b = (a*4 + 3) / kMagicUnkonwnFirst;
    int c = (-b*kMagicUnkonwnFirst)/4 + a;
    int d =((c*4 + 3) / kMagicUnkonwnSec);
    int e = -d * kMagicUnkonwnSec;
    e = e/4 + c;
    int m = (5*e + 2)/kDaysFromYear;
    tm->tm_mday = -(kDaysFromYear * m + 2)/5 + e + 1;
    tm->tm_mon = (-m/10)*12 + m + 2;
    tm->tm_year = b*100 + d  - 6700 + (m/10);
    return 0;
}

int SetBinPath(std::string bin_path, std::string self) {
	// 获取bin目录
	char dir_path[256] = {'\0'};
	if (bin_path.size() == 0) {
        // 可执行文件路径
		int len = readlink(self.c_str(), dir_path, 256);
		if (len < 0 || len >= 256) {
			memcpy(dir_path, kRuntimePath, strlen(kRuntimePath) + 1);
		} else {
			for (int i = len; i >= 0; --i) {
				if (dir_path[i] == '/') {
					dir_path[i+1] = '\0';
					break;
				}
			}
		}
	} else {
		memcpy(dir_path, bin_path.c_str(), bin_path.size() + 1);
	}
    
	// 切换目录
    if (chdir(dir_path) == -1) {
        return -1;
    }
    umask(0);
    return 0;
}

}	// namespace misc

namespace error {

const int32_t kSysNerr = 132;
std::map<int32_t, const std::string> gSysErrlist;

void StrerrorInit() {
	char* msg;
	for (int32_t err = 0; err < kSysNerr; err++) {
		msg = ::strerror(err);
		gSysErrlist.insert(std::make_pair(err, msg));
	}
	gSysErrlist.insert(std::make_pair(kSysNerr, "Unknown error"));
}

const std::string& Strerror(int32_t err) {
	if (err >= 0 && err < kSysNerr) {
		return gSysErrlist[err];
	}
	return gSysErrlist[kSysNerr];
}

}	//namespace error

namespace http {

static struct StatusCode_t statusCodes[] = {
    {"100", "Continue"},
    {"101", "Switching Protocols"},

    {"200", "Ok"},
    {"201", "Created"},
    {"202", "Accepted"},
    {"203", "Non-authoritative Information"},
    {"204", "No Content"},
    {"205", "Reset Content"},
    {"206", "Partial Content"},

    {"300", "Multiple Choices"},
    {"301", "Moved Permanently"},
    {"302", "Found"},
    {"303", "See Other"},
    {"304", "Not Modified"},
    {"305", "Use Proxy"},
    {"306", "Unused"},
    {"307", "Temporary Redirect"},

    {"400", "Bad Request"},
    {"401", "Unauthorized"},
    {"402", "Payment Required"},
    {"403", "Forbidden"},
    {"404", "Not Found"},
    {"405", "Method Not Allowed"},
    {"406", "Not Acceptable"},
    {"407", "Proxy Authentication Required"},
    {"408", "Request Timeout"},
    {"409", "Conflict"},
    {"410", "Gone"},
    {"411", "Length Required"},
    {"412", "Precondition Failed"},
    {"413", "Request Entity Too Large"},
    {"414", "Request-url Too Long"},
    {"415", "Unsupported Media Type"},
    {"416", ""},
    {"417", "Expectation Failed"},

    {"500", "Internal Server Error"},
    {"501", "Not Implemented"},
    {"502", "Bad Gateway"},
    {"503", "Service Unavailable"},
    {"504", "Gateway Timeout"},
    {"505", "HTTP Version Not Supported"},

    {"", ""}
};

std::map<const std::string, const std::string> gStatusCode;

void StatusCodeInit() {
    for (int i = 0; strlen(statusCodes[i].code); i++) {
        gStatusCode.insert(std::make_pair(statusCodes[i].code, statusCodes[i].status));
    }
}

const std::string& Status(const std::string& code) {
    return gStatusCode[code];
}

static unsigned char toHex(unsigned char x) {
    return  x > 9 ? x + 55 : x + 48;
}

static unsigned char fromHex(unsigned char x) {
    unsigned char y;
    if (x >= 'A' && x <= 'Z') {
        y = x - 'A' + 10;
    } else if (x >= 'a' && x <= 'z') {
        y = x - 'a' + 10;
    } else {
        y = x - '0';
    }
    return y;
}

std::string UrlEncode(const std::string& str) {
    std::string strTemp = "";
    for (size_t i = 0; i < str.length(); i++) {
        if (isalnum(static_cast<unsigned char>(str[i])) || 
            (str[i] == '-') ||
            (str[i] == '_') || 
            (str[i] == '.') || 
            (str[i] == '~')) {
            strTemp += str[i];
        } else if (str[i] == ' ') {
            strTemp += "+";
        } else {
            strTemp += '%';
            strTemp += toHex(static_cast<unsigned char>(str[i]) >> 4);
            strTemp += toHex(static_cast<unsigned char>(str[i]) % 16);
        }
    }
    return strTemp;
}

std::string UrlDecode(const std::string& str) {
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++) {
        if (str[i] == '+') {
            strTemp += ' ';
        } else if (str[i] == '%') {
            if (i+2 >= length) {
                return "";
            }
            unsigned char high = fromHex(static_cast<unsigned char>(str[++i]));
            unsigned char low = fromHex(static_cast<unsigned char>(str[++i]));
            strTemp += high*16 + low;
        } else {
            strTemp += str[i];
        }
    }
    return strTemp;
}

}   // namespace http

namespace soft {
static wAtomic<int64_t> hnet_currentTime(0);

int64_t TimeUpdate() {
    int64_t tm = misc::GetTimeofday();
    hnet_currentTime.ReleaseStore(tm);
    return tm;
}
int64_t TimeUsec() {
    return hnet_currentTime.AcquireLoad();
}
time_t TimeUnix() {
    return static_cast<time_t>(hnet_currentTime.AcquireLoad()/1000000);
}

static uid_t hnet_deamonUser = kDeamonUser;
static gid_t hnet_deamonGroup = kDeamonGroup;
static std::string  hnet_softwareName = kSoftwareName;
static std::string  hnet_softwareVer = kSoftwareVer;

static std::string  hnet_runtimePath = kRuntimePath;
static std::string  hnet_logdirPath = kLogdirPath;

static std::string  hnet_acceptFilename = kAcceptFilename;
static std::string  hnet_lockFilename = kLockFilename;
static std::string  hnet_pidFilename = kPidFilename;
static std::string  hnet_logFilename = kLogFilename;

static std::string  hnet_acceptFullPath = hnet_runtimePath + kAcceptFilename;
static std::string  hnet_lockFullPath = hnet_runtimePath + kLockFilename;
static std::string  hnet_pidFullPath = hnet_runtimePath + kPidFilename;
static std::string  hnet_logFullPath = hnet_logdirPath + kLogFilename;

// 设置运行用户
uid_t SetUser(uid_t uid) { return hnet_deamonUser = uid;}
gid_t SetGroup(gid_t gid) { return hnet_deamonGroup = gid;}

// 设置版本信息
const std::string& SetSoftName(const std::string& name) { return hnet_softwareName = name;}
const std::string& SetSoftVer(const std::string& ver) { return hnet_softwareVer = ver;}

// 设置路径
void SetRuntimePath(const std::string& path) {
    hnet_runtimePath = path;
    hnet_acceptFullPath = path + hnet_acceptFilename;
    hnet_lockFullPath = path + hnet_lockFilename;
    hnet_pidFullPath = path + hnet_pidFilename;
}

void SetLogdirPath(const std::string& path) {
    hnet_logdirPath = path;
    hnet_logFullPath = path + hnet_logFilename;
}

// 设置文件名
void SetAcceptFilename(const std::string& filename) {
    hnet_acceptFilename = filename;
    hnet_acceptFullPath = hnet_runtimePath + filename;
}

void SetLockFilename(const std::string& filename) {
    hnet_lockFilename = filename;
    hnet_lockFullPath = hnet_runtimePath + filename;
}

void SetPidFilename(const std::string& filename) {
    hnet_pidFilename = filename;
    hnet_pidFullPath = hnet_runtimePath + filename;
}

void SetLogFilename(const std::string& filename) {
    hnet_logFilename = filename;
    hnet_logFullPath = hnet_logdirPath + filename;
}

uid_t GetUser() { return hnet_deamonUser;}
gid_t GetGroup() { return hnet_deamonGroup;}
const std::string& GetSoftName() { return hnet_softwareName;}
const std::string& GetSoftVer() { return hnet_softwareVer;}
const std::string& GetRuntimePath() { return hnet_runtimePath;}
const std::string& GetLogdirPath() { return hnet_logdirPath;}
const std::string& GetAcceptPath(bool fullpath) { return fullpath? hnet_acceptFullPath: hnet_acceptFilename;}
const std::string& GetLockPath(bool fullpath) { return fullpath? hnet_lockFullPath: hnet_lockFilename;}
const std::string& GetPidPath(bool fullpath) { return fullpath? hnet_pidFullPath: hnet_pidFilename;}
const std::string& GetLogPath(bool fullpath) { return fullpath? hnet_logFullPath: hnet_logFilename;}

}	// namespace hnet

}	// namespace hnet
