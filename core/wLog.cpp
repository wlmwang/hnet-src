
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wLog.h"

#ifdef USE_LOG4CPP
#   ifdef LINUX
#       ifndef LOG4CPP_HAVE_GETTIMEOFDAY 
#           define LOG4CPP_HAVE_GETTIMEOFDAY
#   endif
#   endif

#   include "log4cpp/Category.hh"
#   include "log4cpp/RollingFileAppender.hh"
#   include "log4cpp/BasicLayout.hh"
#   include "log4cpp/PatternLayout.hh"
#endif

namespace hnet {

//初始一种类型的日志
int InitLog(const char* vLogName, const char* vLogDir, int32_t vPriority, uint32_t vMaxFileSize, uint32_t vMaxBackupIndex, bool vAppend) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName || NULL == vLogDir) return -1;

    log4cpp::PatternLayout* tpLayout = new log4cpp::PatternLayout();
    tpLayout->setConversionPattern("%d: %p: %m%n");
    log4cpp::Appender* tpAppender = new log4cpp::RollingFileAppender(vLogName, vLogDir, vMaxFileSize, vMaxBackupIndex, vAppend);
    tpAppender->setLayout(tpLayout);
    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);

    if (tpCategory != NULL) {
            tpCategory->removeAllAppenders();
            tpCategory->setAdditivity(false);
            tpCategory->setAppender(tpAppender);
            tpCategory->setPriority((log4cpp::Priority::PriorityLevel) vPriority );
    } else {
            log4cpp::Category& trCategory = log4cpp::Category::getInstance(vLogName);
            trCategory.setAdditivity(false);
            trCategory.setAppender(tpAppender);
            trCategory.setPriority((log4cpp::Priority::PriorityLevel) vPriority);
    }
#endif
    return 0;
}

int ReInitLog(const char* vLogName, int32_t vPriority, uint32_t vMaxFileSize, uint32_t vMaxBackupIndex) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;
    log4cpp::Category* tpCategory = log4cpp::Category::exists( vLogName);
    if (tpCategory != NULL) {
            log4cpp::RollingFileAppender* pAppender = (log4cpp::RollingFileAppender*) tpCategory->getAppender();

            if (pAppender == NULL) return -1;
            pAppender->setMaxBackupIndex(vMaxBackupIndex);
            pAppender->setMaximumFileSize(vMaxFileSize);
            tpCategory->setPriority(vPriority);
    }
#endif
    return 0;
}

int ShutdownAllLog() {
#ifdef USE_LOG4CPP
    log4cpp::Category::shutdown();
#endif
    return 0;
}

int Log(const char* vLogName, int vPriority, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva((log4cpp::Priority::PriorityLevel)vPriority, vFmt, va);
    va_end(va);
#endif
    return 0;
}

int LogDebug(const char* vLogName, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva(log4cpp::Priority::DEBUG, vFmt, va);
    va_end(va);
#endif
    return 0;	
}

int LogInfo(const char* vLogName, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva(log4cpp::Priority::INFO, vFmt, va);
    va_end(va);
#endif
    return 0;	
}

int LogNotice(const char* vLogName, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva(log4cpp::Priority::NOTICE, vFmt, va);
    va_end(va);
#endif
    return 0;	
}

int LogWarn(const char* vLogName, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva(log4cpp::Priority::WARN, vFmt, va);
    va_end(va);
#endif
    return 0;
}

int LogError(const char* vLogName, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva(log4cpp::Priority::ERROR, vFmt, va);
    va_end(va);
#endif
    return 0;
}

int LogFatal(const char* vLogName, const char* vFmt, ... ) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    va_list va;
    va_start(va, vFmt);
    tpCategory->logva(log4cpp::Priority::FATAL, vFmt, va);
    va_end(va);
#endif
    return 0;	
}

int LogDebug_va(const char* vLogName, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(log4cpp::Priority::DEBUG, vFmt, ap);
#endif
    return 0;
}

int LogNotice_va(const char* vLogName, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(log4cpp::Priority::NOTICE, vFmt, ap);
#endif
    return 0;
}

int LogInfo_va(const char* vLogName, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(log4cpp::Priority::INFO, vFmt, ap);
#endif
    return 0;
}

int LogWarn_va(const char* vLogName, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(log4cpp::Priority::WARN, vFmt, ap);
#endif
    return 0;
}

int LogError_va(const char* vLogName, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(log4cpp::Priority::ERROR, vFmt, ap);
#endif
    return 0;
}

int LogFatal_va(const char* vLogName, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(log4cpp::Priority::FATAL, vFmt, ap);
#endif
    return 0;
}

int Log_va(const char* vLogName, int vPriority, const char* vFmt, va_list ap) {
#ifdef USE_LOG4CPP
    if (NULL == vLogName) return -1;

    log4cpp::Category* tpCategory = log4cpp::Category::exists(vLogName);
    if (NULL == tpCategory) return -1;

    tpCategory->logva(vPriority, vFmt, ap);
#endif
    return 0;
}

void Log_bin(const char* vLogName, void* vBin, int vBinLen) {
#ifdef USE_LOG4CPP
    char* pBuffer = (char*)vBin;
    char tmpBuffer[60000] = {0};
    char strTemp[32] = {0};

    for (int i = 0; i < vBinLen; i++ ) {
            if (!(i % 16)) {
                    sprintf(strTemp, "\n%04d>    ", i / 16 + 1);
                    strcat(tmpBuffer, strTemp);
            }
            sprintf(strTemp, "%02X ", (unsigned char)pBuffer[i]);
            strcat(tmpBuffer, strTemp);
    }
    LogDebug(vLogName, tmpBuffer );
#endif
}

}	// namespace hnet