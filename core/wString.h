
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

/**
 *  用于实现一个在共享内存下的最大长度固定的字符串
 */

#ifndef _W_STRING_H_
#define _W_STRING_H_

#include <stdarg.h>
#include <ostream>

#include "wCore.h"

namespace W
{
	template <int size>
	class string
	{
		public:
			string()
			{
				memset(&value_, 0, MAX_SIZE+1);
				len_ = 0;
			}

			string(const string& s)
			{
				strcpy(value_, s.value_);
				len_ = s.len_;
			}

			string(const char *s)
			{
				strncpy(value_, s, MAX_SIZE);
				value_[MAX_SIZE] = '\0';
				short len = strlen(s);
				len_ = len > MAX_SIZE ? MAX_SIZE : len;
			}

			string(char c)
			{
				value_[0] = c;
				value_[1] = 0;
				len_ = 1;
			}

			inline short length() { return len_; }

			inline const char *c_str() const
			{
				return value_;
			}

			inline string& append(const char *fmt, ...)
			{
				va_list ap;
				va_start(ap, fmt);
				int n = vsnprintf(&value_[len_], MAX_SIZE - len_, fmt, ap);
				va_end(ap);

				if( n >= (MAX_SIZE - len_) ) 
				{
					len_ = MAX_SIZE - 1;
				}
				else
				{
					len_ += n;
				}
				return *this;
			}

			inline string& append(const string& s)
			{
				strncat(value_, s.value_, MAX_SIZE - value_);
				len_ = strlen(value_);

				return *this;
			}

			inline string& append(char c)
			{
				if( len_ < MAX_SIZE )
				{
					value_[len_] = c;
					value_[++len_] = 0;
				}
				return *this;
			}

			friend std::ostream& operator<< (std::ostream& os, string& s)
			{
				os << s.value_;
				return os;
			}

			inline friend string operator+ (const string& lhs, const string &rhs)
			{
				return string(lhs).append(rhs);
			}

			inline friend string operator+ (const char *lhs, const string &rhs)
			{
				return string(lhs).append(rhs);
			}

			inline friend string operator+ (const string& lhs, const char *rhs)
			{
				return string(lhs).append(rhs);
			}

			inline friend string operator+ (char lhs, const string &rhs)
			{
				return string(lhs).append(rhs);
			}

			inline friend string operator+ (const string& lhs, char rhs)
			{
				return string(lhs).append(string(rhs));
			}

			inline string& operator+= (const string& s)
			{
				return append(s);
			}

			inline string& operator+= (const char *s)
			{
				return append(s);
			}

			inline char operator[] (short index)
			{
				if( index <= len_ )
				{
					return value_[index];
				}
				return 0;
			}

			inline string& operator= (const string& s)
			{
				if( this != &s )
				{
					strcpy(value_, s.value_);
					len_ = s.len_;
				}
				return *this;
			}

			inline bool operator== (const string& s) const
			{
				if( this != &s )
				{
					if( this != &s )
					{
						if( len_ != s.len_ )
						{
							return false;
						}
						return (strcmp(value_, s.value_) == 0);
					}
					return true;
				}
			}

			inline bool operator< (const string& s) const
			{
				if( this != &s )
				{
					if( len_ < s.len_ )
					{
						return true;
					}

					if( len_ > s.len_ )
					{
						return false;
					}

					return (strcmp(value_, s.value_) < 0);
				}
				return false;
			}

			inline bool operator> (const string& s) const
			{
				if( this != &s )
				{
					if( len_ > s.len_ )
					{
						return true;
					}

					if( len_ < s.len_ )
					{
						return false;
					}

					return (strcmp(value_, s.value_) > 0);
				}

				return false;
			}

		private:
			enum {MAX_SIZE = size};
			char value_[MAX_SIZE + 1];
			short len_;
	};
};

#endif
