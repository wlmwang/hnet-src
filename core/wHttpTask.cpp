
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include <algorithm>
#include <vector>
#include "wHttpTask.h"
#include "wMisc.h"
#include "wLogger.h"

namespace hnet {

const wStatus& wHttpTask::TaskRecv(ssize_t *size) {
	const char *buffend = mRecvBuff + kPackageSize;
	// 循环队列
	if (mRecvLen < kPackageSize && mRecvLen == 0) {
		mRecvRead = mRecvWrite = mRecvBuff;
	} else if (mRecvLen < kPackageSize && mRecvRead != mRecvBuff && mRecvWrite == buffend) {
		mRecvWrite = mRecvBuff;
	}
	*size = 0;

    // len >= 0 正向剩余
    // len < 0 分段，中间剩余
    ssize_t len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
    size_t leftlen = len >= 0? static_cast<size_t>(buffend - mRecvWrite): static_cast<size_t>(abs(len));

    if (mRecvLen < kPackageSize) {
    	if (len >= 0 && leftlen <= 10*sizeof(kProtocol[0])) {
    		// 队列太过靠后，重新调整
    		memmove(mRecvBuff, mRecvRead, len);
    		mRecvRead = mRecvBuff;
    		mRecvWrite= mRecvBuff + len;
    		len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
    		leftlen = len>=0? static_cast<size_t>(buffend - mRecvWrite): static_cast<size_t>(abs(len));
    	}

    	if (leftlen > 0) {
        	// socket接受数据
        	if (!(mStatus = mSocket->RecvBytes(mRecvWrite, leftlen, size)).Ok() || *size < 0) {
                return mStatus;
            }
            mRecvLen += *size;
            mRecvWrite += *size;
    	}
    }

    // 消息解析
    auto handleFunc = [this] (char* buf, uint32_t len, char* nextbuf) {
        this->mStatus = this->Handlemsg(buf, len);
        this->mRecvRead = nextbuf;
        this->mRecvLen -= len;
    };

    while (mRecvLen > strlen(kProtocol[0]) + strlen(kMethod[0]) + strlen(kCRLF)) {
    	// 循环队列
    	if (mRecvRead == buffend && mRecvWrite != mRecvBuff) {
            mRecvRead = mRecvBuff;
        }
    	ssize_t len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
        
        uint32_t reallen = 0;
        if (len > 0) {
            // 消息字符在正向缓冲中
           	if (misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[0])), kMethod[0], strlen(kMethod[0])) == 0) {
           		// GET请求
           		int32_t pos = misc::Strpos(std::string(mRecvRead, 0, len), kEndl);
           		if (pos == -1) {
           			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv, [GET] recv a part of message", ">0");
           			mStatus.Clear();
	                break;
           		}

           		reallen = pos + strlen(kEndl);
           	} else if (misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[1])), kMethod[1], strlen(kMethod[1])) == 0) {
           		// POST请求
           		int32_t pos = misc::Strpos(std::string(mRecvRead, 0, len), kEndl);
           		if (pos == -1) {
	                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv, [POST] recv a part of message", ">0");
	                mStatus.Clear();
	                break;
           		}

           		int32_t pos1 = misc::Strpos(std::string(mRecvRead, 0, len), kHeader[0]);
           		if (pos1 == -1) {
           			mStatus = wStatus::Corruption("wHttpTask::TaskRecv, [POST] header error(has no Content-Length)", ">0");
           			break;
           		}
           		int32_t contentLength = atoi(mRecvRead + pos1 + strlen(kHeader[0]) + strlen(kColon));
           		if (pos + strlen(kEndl) + contentLength > kMaxPackageSize) {
           			mStatus = wStatus::Corruption("wHttpTask::TaskRecv, [POST] header error(Content-Length out range)", ">0");
           			break;
           		} else if (pos + strlen(kEndl) + contentLength > static_cast<uint32_t>(len)) {
	                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv, [POST] recv a part of message", ">0");
	                mStatus.Clear();
	                break;
           		}

           		reallen = pos + strlen(kEndl) + contentLength;
           	} else {
           		// 未知请求
                mStatus = wStatus::Corruption("wHttpTask::TaskRecv, method error", ">0");
                break;
           	}

            handleFunc(mRecvRead, reallen, mRecvRead + reallen);
            if (!mStatus.Ok()) {
                break;
            }
        } else {
            leftlen = buffend - mRecvRead;
            if ((leftlen >= strlen(kMethod[0]) && misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[0])), kMethod[0], strlen(kMethod[0])) == 0) ||
            	(leftlen == 0 && misc::Strcmp(std::string(mRecvBuff, 0, strlen(kMethod[0])), kMethod[0], strlen(kMethod[0])) == 0)) {
           		// GET请求
           		int32_t pos = misc::Strpos(std::string(mRecvRead, 0, leftlen), kEndl);
                if (pos == -1) {
                	memcpy(mTempBuff, mRecvRead, leftlen);
                	memcpy(mTempBuff + leftlen, mRecvBuff, mRecvLen - leftlen);
                	pos = misc::Strpos(std::string(mTempBuff, 0, mRecvLen), kEndl);
	           		if (pos == -1) {
	           			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv, [GET] recv a part of message", "<=0");
	           			mStatus.Clear();
		                break;
	           		}

	           		reallen = leftlen + pos + strlen(kEndl);
                } else {
                	reallen = pos + strlen(kEndl);
                }

                if (reallen <= leftlen) {
                	handleFunc(mRecvRead, reallen, mRecvRead + reallen);
                } else {
                	handleFunc(mTempBuff, reallen, mRecvBuff + reallen - leftlen);
                }
                if (!mStatus.Ok()) {
                    break;
                }
            } else if ((leftlen >= strlen(kMethod[1]) && misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[1])), kMethod[1], strlen(kMethod[1])) == 0) || 
            	(leftlen == 0 && misc::Strcmp(std::string(mRecvBuff, 0, strlen(kMethod[1])), kMethod[1], strlen(kMethod[1])) == 0)) {
            	// POST
            	memcpy(mTempBuff, mRecvRead, leftlen);
            	memcpy(mTempBuff + leftlen, mRecvBuff, mRecvLen - leftlen);

           		int32_t pos = misc::Strpos(std::string(mTempBuff, 0, mRecvLen), kEndl);
           		if (pos == -1) {
	                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv, [POST] recv a part of message", "<=0");
	                mStatus.Clear();
	                break;
           		}

           		int32_t pos1 = misc::Strpos(std::string(mTempBuff, 0, mRecvLen), kHeader[0]);
           		if (pos1 == -1) {
           			mStatus = wStatus::Corruption("wHttpTask::TaskRecv, [POST] header error(has no Content-Length)", "<=0");
           			break;
           		}
           		int32_t contentLength = atoi(mTempBuff + pos1 + strlen(kHeader[0]) + strlen(kColon));
           		if (pos + strlen(kEndl) + contentLength > kMaxPackageSize) {
           			mStatus = wStatus::Corruption("wHttpTask::TaskRecv, [POST] header error(Content-Length out range)", "<=0");
           			break;
           		} else if (pos + strlen(kEndl) + contentLength > static_cast<uint32_t>(mRecvLen)) {
	                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv, [POST] recv a part of message", "<=0");
	                mStatus.Clear();
	                break;
           		}

           		reallen = pos + strlen(kEndl) + contentLength;
                if (reallen <= leftlen) {
                	handleFunc(mTempBuff, reallen, mRecvRead + reallen);
                } else {
                	handleFunc(mTempBuff, reallen, mRecvBuff + reallen - leftlen);
                }
                if (!mStatus.Ok()) {
                    break;
                }
            }
        }
    }
    return mStatus;
}

const wStatus& wHttpTask::Handlemsg(char buf[], uint32_t len) {
	mReq.clear(); mRes.clear(); mGet.clear(); mPost.clear();
	ParseRequest(buf, len);
	std::string cmd = QueryGet(kCmd[0]);
	std::string para = QueryGet(kCmd[1]);
	if (!cmd.empty() && !para.empty()) {
		struct Request_t request(buf, len);
		if (mEventCmd(CmdId(atoi(cmd.c_str()), atoi(para.c_str())), &request) == false) {
			mStatus = wStatus::Corruption("wTask::Handlemsg, invalid request, no method find", "cmd:" + cmd + ", para:" + para);
		}
	} else {
		mStatus = wStatus::Corruption("wTask::Handlemsg, invalid request, no cmd or para find", "");
	}
	return ParseResponse();
}

const wStatus& wHttpTask::ParseRequest(char buf[], uint32_t len) {
	std::vector<std::string> req = misc::SplitString(std::string(buf, 0, len), kCRLF);
	if (!req.empty()) {	// 请求行
		std::vector<std::string> line = misc::SplitString(req.front(), " ");
		if (line.size() == 3) {
			mReq.insert(std::make_pair(kLine[0], line[0]));
			mReq.insert(std::make_pair(kLine[1], line[1]));
			mReq.insert(std::make_pair(kLine[2], line[2]));
			// 请求参数
			std::vector<std::string> line1 = misc::SplitString(line[1], "?");
			if (line1.size() == 1) {
				mReq.insert(std::make_pair(kLine[3], line1[0]));
			} else if (line1.size() > 1) {
				mReq.insert(std::make_pair(kLine[3], line1[0]));
				mReq.insert(std::make_pair(kLine[4], "?"+line1.back()));
				mReq.insert(std::make_pair(kLine[5], line1.back()));
				// get
				std::vector<std::string> line2 = misc::SplitString(mReq[kLine[5]], "&");
				if (!line2.empty()) {
					for (std::vector<std::string>::iterator it2 = line2.begin(); it2 != line2.end(); it2++) {
						std::vector<std::string> v = misc::SplitString(*it2, "=");
						if (v.size() == 2) {
							mGet[v[0]] = v[1];
						} else if (v.size() == 1) {
							mGet[v[0]] = "";
						}
					}
				}
			}
		}
		req.erase(req.begin());
	}

	int8_t p = 0;
	for (std::vector<std::string>::iterator it = req.begin(); it != req.end(); it++) {	// header + 参数
		if (it->size() == 0) {	// header与body分隔
			p = 1;
			continue;
		}
		if (p == 0) {
			std::vector<std::string> header = misc::SplitString(*it, kColon);
			if (header.size() == 2) {
				mReq.insert(std::make_pair(header[0], header[1]));
			}
		} else {
			mReq.insert(std::make_pair(kLine[6], *it));
			// post
			std::vector<std::string> line2 = misc::SplitString(mReq[kLine[6]], "&");
			if (!line2.empty()) {
				for (std::vector<std::string>::iterator it2 = line2.begin(); it2 != line2.end(); it2++) {
					std::vector<std::string> v = misc::SplitString(*it2, "=");
					if (v.size() == 2) {
						mPost[v[0]] = v[1];
					} else if (v.size() == 1) {
						mPost[v[0]] = "";
					}
				}
			}
		}
	}
	return mStatus;
}

const wStatus& wHttpTask::ParseResponse() {
    AssertResponse();
    const char *buffend = mSendBuff + kPackageSize;
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    ssize_t len = 0;
    std::string tmp;

    // 响应行
    tmp = mRes[kLine[2]] + " " + mRes[kLine[8]] + " " + mRes[kLine[7]] + kCRLF;
	memcpy(mTempBuff, tmp.c_str(), tmp.size());
	len = tmp.size();

	// header
	for (std::map<std::string, std::string>::iterator it = mRes.begin(); it != mRes.end(); it++) {
		if (it->first == kLine[2] || it->first == kLine[7] || it->first == kLine[8] || it->first == kLine[9]) {
			continue;
		}

		tmp = it->first + kColon + it->second + kCRLF;
		memcpy(mTempBuff + len, tmp.c_str(), tmp.size());
		len += tmp.size();
	}
	tmp = kCRLF;
	memcpy(mTempBuff + len, tmp.c_str(), tmp.size());
	len += tmp.size();
	
	// 响应body
	if (!mRes[kLine[9]].empty()) {
		tmp = mRes[kLine[9]];
		memcpy(mTempBuff + len, tmp.c_str(), tmp.size());
		len += tmp.size();
	}

    if ((writelen >= 0 && leftlen >= static_cast<ssize_t>(len)) || (writelen < 0 && abs(writelen) >= static_cast<ssize_t>(len))) {
    	// 单向剩余足够（右边剩余 || 中间剩余）
    	memcpy(mSendWrite, mTempBuff, len);
    	mSendWrite += len;
    } else if (writelen >= 0 && leftlen < static_cast<ssize_t>(len + sizeof(uint32_t))) {
    	// 分段剩余足够（两边剩余）
    	memcpy(mSendWrite, mTempBuff, leftlen);
    	memcpy(mSendBuff, mTempBuff + leftlen, len - leftlen);
    	mSendWrite = mSendBuff + len - leftlen;
    } else {
    	return mStatus = wStatus::Corruption("wHttpTask::ParseResponse, message length error", "left buffer not enough");
    }
    mSendLen += len;
	return mStatus = Output();
}

void wHttpTask::AssertResponse() {
	if (!mReq[kHeader[3]].empty()) {	// keep-alive
		ResponseSet(kHeader[3], mReq[kHeader[3]]);
		ResponseSet(kHeader[7], "timeout=30, max=120");
	} else {
		ResponseSet(kHeader[3], "close");
	}

    ResponseSet(kLine[2], kProtocol[0]);	// HTTP/1.1
    if (mRes[kLine[7]].empty()) {	// code
    	ResponseSet(kLine[7], "200");
    }
    if (mRes[kLine[8]].empty()) {	// status
    	ResponseSet(kLine[8], "Ok");
    }
	if (!mRes[kLine[9]].empty()) {	// Content-Length
		ResponseSet(kHeader[0], logging::NumberToString(static_cast<uint64_t>(mRes[kLine[9]].size())));
	} else {
		ResponseSet(kHeader[0], "0");
	}
	if (mRes[kHeader[1]].empty()) {	// Content-Type
		ResponseSet(kHeader[1], "text/html; charset=UTF-8");
	}
	if (mRes[kHeader[4]].empty()) {	// X-Powered-By
		ResponseSet(kHeader[4], soft::GetSoftName() + "/" + soft::GetSoftVer());
	}
	if (mRes[kHeader[5]].empty()) {	// Cache-Control
		ResponseSet(kHeader[5], "no-store, no-cache, must-revalidate");
	}
	if (mRes[kHeader[6]].empty()) {	// Pragma
		ResponseSet(kHeader[6], "no-cache");
	}
}

void wHttpTask::Error(const std::string& status, const std::string& code) {
	std::string s = status, c = code;
	if (s.empty()) {
		s = HttpStatus(c);
		if (s.empty()) {
			c = "500";
			s = HttpStatus(c);
		}
	}
	ResponseSet(kLine[7], c);
	ResponseSet(kLine[8], s);
}

void wHttpTask::Write(const std::string& body) {
	ResponseSet(kLine[9], body);
}

}	// namespace hnet
