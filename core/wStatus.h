
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_STATUS_H_
#define _W_STATUS_H_

#include "wCore.h"
#include "wSlice.h"

namespace hnet {

class wStatus {
public:
    wStatus() : mState(NULL) { }
    wStatus(const wStatus& s);
    
    ~wStatus() {
        SAFE_DELETE_VEC(mState);
    }

    static wStatus Nothing() {
        return wStatus();
    }

    static wStatus NotFound(const wSlice& msg, const wSlice& msg2 = wSlice()) {
        return wStatus(kNotFound, msg, msg2);
    }

    static wStatus Corruption(const wSlice& msg, const wSlice& msg2 = wSlice()) {
        return wStatus(kCorruption, msg, msg2);
    }

    static wStatus NotSupported(const wSlice& msg, const wSlice& msg2 = wSlice()) {
        return wStatus(kNotSupported, msg, msg2);
    }

    static wStatus InvalidArgument(const wSlice& msg, const wSlice& msg2 = wSlice()) {
        return wStatus(kInvalidArgument, msg, msg2);
    }

    static wStatus IOError(const wSlice& msg, const wSlice& msg2 = wSlice()) {
        return wStatus(kIOError, msg, msg2);
    }
    
    static wStatus AccessIllegal(const wSlice& msg, const wSlice& msg2 = wSlice()) {
        return wStatus(kAccessIllegal, msg, msg2);
    }

    bool Ok() const {
        return mState == NULL;
    }

    bool IsNotFound() const {
        return code() == kNotFound;
    }

    bool IsCorruption() const {
        return code() == kCorruption;
    }

    bool IsIOError() const {
        return code() == kIOError;
    }

    std::string ToString() const;
    
    void operator=(const wStatus& s);

private:
    enum Code {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kNotSupported = 3,
        kInvalidArgument = 4,
        kIOError = 5,
        kAccessIllegal = 6
    };

    Code code() const {
        return (mState == NULL) ? kOk : static_cast<Code>(mState[4]);
    }

    wStatus(Code code, const Slice& msg, const Slice& msg2);

    static const char* CopyState(const char* s);

private:
    // OK status has a NULL mState.  Otherwise, mState is a new[] array
    // of the following form:
    // mState[0..3] == length of message
    // mState[4]    == code
    // mState[5..]  == message
    const char* mState;
};

inline wStatus::wStatus(const wStatus& s) {
    mState = (s.mState == NULL) ? NULL : CopyState(s.mState);
}

inline void wStatus::operator=(const wStatus& s) {
    if (mState != s.mState) {
        SAFE_DELETE_VEC(mState);
        mState = (s.mState == NULL) ? NULL : CopyState(s.mState);
    }
}

}   // namespace hnet

#endif
