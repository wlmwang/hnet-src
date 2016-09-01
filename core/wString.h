
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
    wSlice() : data_(""), size_(0) { }
    
    wSlice(const char* d, size_t n) : data_(d), size_(n) { }

    wSlice(const std::string& s) : data_(s.data()), size_(s.size()) { }

    wSlice(const char* s) : data_(s), size_(strlen(s)) { }

    const char* data() const { return data_; }

    size_t size() const { return size_; }

    bool empty() const { return size_ == 0; }

    char operator[](size_t n) const {
        assert(n < size());
        return data_[n];
    }

    // Change this slice to refer to an empty array
    void clear() { data_ = ""; size_ = 0; }

    // Drop the first "n" bytes from this slice.
    void remove_prefix(size_t n) {
        assert(n <= size());
        data_ += n;
        size_ -= n;
    }

    // Return a string that contains the copy of the referenced data.
    std::string ToString() const { return std::string(data_, size_); }

    // Three-way comparison.  Returns value:
    //   <  0 iff "*this" <  "b",
    //   == 0 iff "*this" == "b",
    //   >  0 iff "*this" >  "b"
    int compare(const Slice& b) const;

    // Return true iff "x" is a prefix of "*this"
    bool starts_with(const Slice& x) const {
        return ((size_ >= x.size_) && (memcmp(data_, x.data_, x.size_) == 0));
    }

private:
    const char* data_;
    size_t size_;

 // Intentionally copyable
};

inline bool operator==(const Slice& x, const Slice& y) {
    return ((x.size() == y.size()) && (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) {
    return !(x == y);
}

inline int Slice::compare(const Slice& b) const {
    const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
    int r = memcmp(data_, b.data_, min_len);
    if (r == 0) {
      if (size_ < b.size_) r = -1;
      else if (size_ > b.size_) r = +1;
    }
    return r;
}

}   // namespace hnet

#endif
