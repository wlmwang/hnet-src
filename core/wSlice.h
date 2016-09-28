
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_SLICE_H_
#define _W_SLICE_H_

#include <cstdarg>
#include "wCore.h"

namespace hnet {

class wSlice {
public:
    wSlice() : mData(""), mSize(0) { }

    wSlice(const char* d, size_t n) : mData(d), mSize(n) { }

    wSlice(const std::string& s) : mData(s.data()), mSize(s.size()) { }
    
    wSlice(const char* s) : mData(s), mSize(strlen(s)) { }

    const char* data() const { 
	   return mData; 
    }
    
    size_t size() const { 
	   return mSize; 
    }
    
    bool empty() const {
	   return mSize == 0; 
    }
    
    char operator[](size_t n) const {
        assert(n < size());
        return mData[n];
    }
    
    void clear() { 
    	mData = "";
    	mSize = 0; 
    }

    void removePrefix(size_t n) {
        assert(n <= size());
        mData += n;
        mSize -= n;
    }
    
    bool startsWith(const wSlice& x) const {
        return ((mSize >= x.mSize) && (memcmp(mData, x.mData, x.mSize) == 0));
    }
    
    std::string ToString() const { 
	   return std::string(mData, mSize);
    }
    
    // <  0 iff "*this" <  "b",
    // == 0 iff "*this" == "b",
    // >  0 iff "*this" >  "b"
    int compare(const wSlice& b) const;

private:
    const char* mData;
    size_t mSize;
};

inline bool operator==(const wSlice& x, const wSlice& y) {
    return ((x.size() == y.size()) && (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const wSlice& x, const wSlice& y) {
    return !(x == y);
}

inline int wSlice::compare(const wSlice& b) const {
    const size_t minLen = (mSize < b.mSize) ? mSize : b.mSize;
    int r = memcmp(mData, b.mData, minLen);
    if (r == 0) {
        if (mSize < b.mSize) {
            r = -1;
    	} else if (mSize > b.mSize) {
            r = +1;
    	}
    }
    return r;
}

}   // namespace hnet

#endif