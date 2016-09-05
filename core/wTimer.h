
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_TIMER_H_
#define _W_TIMER_H_

#include "wCore.h"

namespace hnet {
class wTimer {
public:
    wTimer() : mTimer(0), mTimeRecord(0) { }
    wTimer(int vTimer): mTimer(vTimer), mTimeRecord(vTimer) { }

    bool CheckTimer(int vInterval) {
        int vPassTime = mTimer - vInterval;
        if (vPassTime <= 0) {
            // 补差值时间
            mTimer = mTimeRecord + vPassTime;
            return true;
        } else {
            mTimer = vPassTime;
            return false;
        }
    }

private:
    int64_t mTimer;
    int64_t mTimeRecord;
};

}	// namespace hnet

#endif
