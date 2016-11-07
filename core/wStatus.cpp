
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wCore.h"
#include "wStatus.h"
#include "wLogger.h"

namespace hnet {

// 复制状态信息
const char* wStatus::CopyState(const char* state) {
    uint32_t size;
    char* result;
    memcpy(&size, state, sizeof(size));
    SAFE_NEW_VEC(size + 5, char, result);
    assert(result != NULL);
    memcpy(result, state, size + 5);
    return result;
}

wStatus::wStatus(Code code, const wSlice& msg, const wSlice& msg2, bool log) {
    assert(code != kOk);
    const uint32_t len1 = msg.size();
    const uint32_t len2 = msg2.size();
    const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
    char* result;
    SAFE_NEW_VEC(size + 5, char, result);
    assert(result != NULL);
    memcpy(result, &size, sizeof(size));
    result[4] = static_cast<char>(code);
    memcpy(result + 5, msg.data(), len1);
    if (len2) {
        result[5 + len1] = ':';
        result[6 + len1] = ' ';
        memcpy(result + 7 + len1, msg2.data(), len2);
    }
    mState = result;
    // 日志
    if (log) {
    	LOG_ERROR(kLogPath, "%s", ToString().c_str());
    }
}

std::string wStatus::ToString() const {
    if (mState == NULL) {
        return "OK";
    } else {
        char tmp[30];
        const char* type;
        switch (code()) {
            case kOk:
                type = "OK";
                break;
            case kNotFound:
                type = "NotFound: ";
                break;
            case kCorruption:
                type = "Corruption: ";
                break;
            case kNotSupported:
                type = "Not implemented: ";
                break;
            case kInvalidArgument:
                type = "Invalid argument: ";
                break;
            case kIOError:
                type = "IO error: ";
                break;
            default:
                snprintf(tmp, sizeof(tmp), "Unknown code(%d): ", static_cast<int>(code()));
                type = tmp;
                break;
        }
        std::string result(type);
        uint32_t length;
        memcpy(&length, mState, sizeof(length));
        result.append(mState + 5, length);
        return result;
    }
}

}   // namespace hnet
