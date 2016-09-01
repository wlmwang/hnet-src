
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_STATUS_H_
#define _W_STATUS_H_

#include "wCore.h"

namespace hnet {

class wStatus {
public:
    wStatus() : state_(NULL) { }
    wStatus(const wStatus& s);
    ~wStatus() { delete[] state_; }

    void operator=(const wStatus& s);

    static wStatus OK() { return wStatus(); }

    static wStatus NotFound(const Slice& msg, const Slice& msg2 = Slice()) {
        return wStatus(kNotFound, msg, msg2);
    }

    static wStatus Corruption(const Slice& msg, const Slice& msg2 = Slice()) {
        return wStatus(kCorruption, msg, msg2);
    }

    static wStatus NotSupported(const Slice& msg, const Slice& msg2 = Slice()) {
        return wStatus(kNotSupported, msg, msg2);
    }

    static wStatus InvalidArgument(const Slice& msg, const Slice& msg2 = Slice()) {
        return wStatus(kInvalidArgument, msg, msg2);
    }

    static wStatus IOError(const Slice& msg, const Slice& msg2 = Slice()) {
        return wStatus(kIOError, msg, msg2);
    }

    bool ok() const { return (state_ == NULL); }
    bool IsNotFound() const { return code() == kNotFound; }
    bool IsCorruption() const { return code() == kCorruption; }
    bool IsIOError() const { return code() == kIOError; }
    std::string ToString() const;

private:
    enum Code {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kNotSupported = 3,
        kInvalidArgument = 4,
        kIOError = 5
    };

    Code code() const {
        return (state_ == NULL) ? kOk : static_cast<Code>(state_[4]);
    }
    wStatus(Code code, const Slice& msg, const Slice& msg2);
    static const char* CopyState(const char* s);

private:	
    // OK status has a NULL state_.  Otherwise, state_ is a new[] array
    // of the following form:
    //    state_[0..3] == length of message
    //    state_[4]    == code
    //    state_[5..]  == message
    const char* state_;

};

inline wStatus::wStatus(const wStatus& s) {
	state_ = (s.state_ == NULL) ? NULL : CopyState(s.state_);
}

inline void wStatus::operator=(const wStatus& s) {
    if (state_ != s.state_) {
        delete[] state_;
        state_ = (s.state_ == NULL) ? NULL : CopyState(s.state_);
    }
}

}

#endif
