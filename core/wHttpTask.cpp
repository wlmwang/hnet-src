
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

int wHttpTask::TaskRecv(ssize_t *size) {
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
			int ret = mSocket->RecvBytes(mRecvWrite, leftlen, size);
            if (ret == -1 || (ret == 0 && *size < 0)) {
                return ret;
            }

			mRecvLen += *size;
			mRecvWrite += *size;
		}
	}

	// 消息解析
    auto handleFunc = [this] (char* buf, uint32_t len, char* nextbuf) -> int {
        int ret = this->Handlemsg(buf, len);

        this->mRecvRead = nextbuf;
        this->mRecvLen -= len;
        return ret;
    };

    int ret = 0;
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
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", ">0 [GET] recv a part of message");
					ret = 0;
					break;
				}

				reallen = pos + strlen(kEndl);
			} else if (misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[1])), kMethod[1], strlen(kMethod[1])) == 0) {
				// POST请求
				int32_t pos = misc::Strpos(std::string(mRecvRead, 0, len), kEndl);
				if (pos == -1) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", ">0 [POST] recv a part of message");
					ret = 0;
					break;
				}

				int32_t pos1 = misc::Strpos(std::string(mRecvRead, 0, len), kHeader[0]);
				if (pos1 == -1) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", ">0 [POST] header error(has no Content-Length)");
					ret = -1;
					break;
				}

				int32_t contentLength = atoi(mRecvRead + pos1 + strlen(kHeader[0]) + strlen(kColon));
				if (pos + strlen(kEndl) + contentLength > kMaxPackageSize) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", ">0 [POST] header error(Content-Length out range)");
					ret = -1;
					break;
				} else if (pos + strlen(kEndl) + contentLength > static_cast<uint32_t>(len)) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", ">0 [POST] recv a part of message");
					ret = 0;
					break;
				}

				reallen = pos + strlen(kEndl) + contentLength;
			} else {
				// 未知请求
				LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", ">0 method error");
				ret = -1;
				break;
			}

			ret = handleFunc(mRecvRead, reallen, mRecvRead + reallen);
            if (ret == -1) {
                break;
            }
		} else {
			leftlen = buffend - mRecvRead;
			if ((leftlen >= strlen(kMethod[0]) && misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[0])), kMethod[0], strlen(kMethod[0])) == 0) ||
				(leftlen == 0 && misc::Strcmp(std::string(mRecvBuff, 0, strlen(kMethod[0])), kMethod[0], strlen(kMethod[0])) == 0)) {
				// GET请求
				int32_t pos = misc::Strpos(std::string(mRecvRead, 0, leftlen), kEndl);
				if (pos == -1) {
					memset(mTempBuff, 0, sizeof(mTempBuff));
					memcpy(mTempBuff, mRecvRead, leftlen);
					memcpy(mTempBuff + leftlen, mRecvBuff, mRecvLen - leftlen);
					pos = misc::Strpos(std::string(mTempBuff, 0, mRecvLen), kEndl);
					if (pos == -1) {
						LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", "<=0 [GET] recv a part of message");
						ret = 0;
						break;
					}

					reallen = leftlen + pos + strlen(kEndl);
				} else {
					reallen = pos + strlen(kEndl);
				}

				if (reallen <= leftlen) {
					ret = handleFunc(mRecvRead, reallen, mRecvRead + reallen);
				} else {
					ret = handleFunc(mTempBuff, reallen, mRecvBuff + reallen - leftlen);
				}

	            if (ret == -1) {
	                break;
	            }
			} else if ((leftlen >= strlen(kMethod[1]) && misc::Strcmp(std::string(mRecvRead, 0, strlen(kMethod[1])), kMethod[1], strlen(kMethod[1])) == 0) || 
				(leftlen == 0 && misc::Strcmp(std::string(mRecvBuff, 0, strlen(kMethod[1])), kMethod[1], strlen(kMethod[1])) == 0)) {
				// POST请求
				memset(mTempBuff, 0, sizeof(mTempBuff));
				memcpy(mTempBuff, mRecvRead, leftlen);
				memcpy(mTempBuff + leftlen, mRecvBuff, mRecvLen - leftlen);

				int32_t pos = misc::Strpos(std::string(mTempBuff, 0, mRecvLen), kEndl);
				if (pos == -1) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", "<=0 [POST] recv a part of message");
					ret = 0;
					break;
				}

				int32_t pos1 = misc::Strpos(std::string(mTempBuff, 0, mRecvLen), kHeader[0]);
				if (pos1 == -1) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", "<=0 [POST] header error(has no Content-Length)");
					ret = -1;
					break;
				}
				int32_t contentLength = atoi(mTempBuff + pos1 + strlen(kHeader[0]) + strlen(kColon));
				if (pos + strlen(kEndl) + contentLength > kMaxPackageSize) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", "<=0 [POST] header error(Content-Length out range)");
					ret = -1;
					break;
				} else if (pos + strlen(kEndl) + contentLength > static_cast<uint32_t>(mRecvLen)) {
					LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", "<=0 [POST] recv a part of message");
					ret = 0;
					break;
				}

				reallen = pos + strlen(kEndl) + contentLength;
				if (reallen <= leftlen) {
					ret = handleFunc(mTempBuff, reallen, mRecvRead + reallen);
				} else {
					ret = handleFunc(mTempBuff, reallen, mRecvBuff + reallen - leftlen);
				}
	            if (ret == -1) {
	                break;
	            }
			} else {
				// 未知请求
				LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::TaskRecv () failed", "<=0 method error");
				ret = -1;
				break;
			}
		}
	}
	return ret;
}

int wHttpTask::Handlemsg(char buf[], uint32_t len) {
	mReq.clear(); mRes.clear(); mGet.clear(); mPost.clear();

	ParseRequest(buf, len);
	std::string cmd = QueryGet(kCmd[0]);
	std::string para = QueryGet(kCmd[1]);
	if (!cmd.empty() && !para.empty()) {
		struct Request_t request(buf, len);
		if (mEventCmd(CmdId(atoi(cmd.c_str()), atoi(para.c_str())), &request) == false) {
			ResponseSet(kHeader[3], "close");
			Error("Not Found(cmd,para illegal)", "404");
		}
	} else {
		ResponseSet(kHeader[3], "close");
		Error("Bad Request(cmd,para must)", "400");
	}
	return AsyncResponse();
}

int wHttpTask::ParseRequest(char buf[], uint32_t len) {
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
			// POST数据
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
	return 0;
}

int wHttpTask::AsyncResponse() {
    const char *buffend = mSendBuff + kPackageSize;
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    ssize_t len = 0;
    std::string tmp;

    // 填写默认头
    FillResponse();

    // 响应行
    memset(mTempBuff, 0, sizeof(mTempBuff));
    tmp = mRes[kLine[2]] + " " + mRes[kLine[7]] + " " + mRes[kLine[8]] + kCRLF;
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

	// 异步缓冲
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
    	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::ParseResponse () failed", "left buffer not enough");
    	return -1;
    }
    mSendLen += len;

	return Output();
}

int wHttpTask::SyncResponse(char buf[], ssize_t* size, uint32_t timeout) {
	int32_t pos = 0, pos1 = 0, len = 0;
	size_t recvlen = 0;
	bool ok = false;	
	int64_t now = soft::TimeUsec();
	int ret = 0;

	while (true) {
        // 超时时间设置
        if (timeout > 0) {
            double tmleft = static_cast<int64_t>(timeout*1000000) - (soft::TimeUpdate() - now);
            if (tmleft > 0) {
                if (mSocket->SetRecvTimeout(tmleft/1000000) == -1) {
                    return -1;
                }
            } else {
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::SyncRecv () failed", "timeout");
                return -1;
            }
        }

        // 接受消息
        ret = mSocket->RecvBytes(mTempBuff + recvlen, kPackageSize, size);
        if (ret == -1) {
            return -1;
        }

		if (*size > 0) {
            recvlen += *size;
        }

        if (recvlen < strlen(kProtocol[0])) {
            continue;
        }

        if (misc::Strcmp(std::string(mTempBuff, 0, strlen(kProtocol[0])), kProtocol[0], strlen(kProtocol[0])) == 0) {	// HTTP/1.1
       		pos = misc::Strpos(std::string(mTempBuff, 0, recvlen), kEndl);
       		if (pos == -1) {
	            continue;
       		}

	   		pos1 = misc::Strpos(std::string(mTempBuff, 0, recvlen), kHeader[0]);	// Content-Length
	   		if (pos1 == -1) {
	   			continue;
	   		}

	   		len = atoi(mTempBuff + pos1 + strlen(kHeader[0]) + strlen(kColon));
	   		if (pos + strlen(kEndl) + len > kMaxPackageSize) {
	   			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::SyncResponse () failed", "header error(Content-Length out range)");
	   			ret = -1;
	   			break;
	   		} else if (pos + strlen(kEndl) + len > static_cast<uint32_t>(recvlen)) {
	   			continue;
	   		} else {
	   			ok = true;
	   		}
        } else {
       		// 未知协议
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::SyncResponse () failed", "only support HTTP/1.1");
            ret = -1;
            break;
        }

        break;
	}

	if (ok == false) {
		if (ret == 0) {
			LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::SyncResponse () failed", "message invaild");
		}
		return -1;
	}
	*size = recvlen;
	memcpy(buf, mTempBuff , *size);
    return 0;
}

int wHttpTask::AsyncRequest() {
    const char *buffend = mSendBuff + kPackageSize;
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    ssize_t len = 0;
    std::string tmp;

    // 填写默认头
    FillResponse();
    
    // 请求行
    memset(mTempBuff, 0, sizeof(mTempBuff));
    tmp = mRes[kLine[0]] + " " + mRes[kLine[1]] + " " + mRes[kLine[2]] + kCRLF;
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

	// 异步缓冲
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
    	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::AsyncRequest () failed", "left buffer not enough");
    	return -1;
    }
    mSendLen += len;

    return Output();
}

int wHttpTask::SyncRequest(ssize_t* size) {
    ssize_t len = 0;
    std::string tmp;

    // 填写默认头
    FillResponse();
    
    // 请求行
    tmp = mRes[kLine[0]] + " " + mRes[kLine[1]] + " " + mRes[kLine[2]] + kCRLF;
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
	return mSocket->SendBytes(mTempBuff, len, size);
}

void wHttpTask::FillResponse() {
	if (mRes[kHeader[3]].empty() && !mReq[kHeader[3]].empty()) {	// keep-alive
		ResponseSet(kHeader[3], mReq[kHeader[3]]);
		ResponseSet(kHeader[7], "timeout=30, max=120");
	} else if (mRes[kHeader[3]].empty()) {
		ResponseSet(kHeader[3], "close");
	}
    if (mRes[kLine[2]].empty()) {	// HTTP/1.1
    	ResponseSet(kLine[2], kProtocol[0]);
    }
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
	return;
}

void wHttpTask::Error(const std::string& status, const std::string& code) {
	if (!code.empty()) {
		if (status.empty()) {
			const std::string s = http::Status(code);
			if (s.empty()) {
				ResponseSet(kLine[7], "500");
				ResponseSet(kLine[8], http::Status("500"));
			}
			ResponseSet(kLine[7], code);
			ResponseSet(kLine[8], s);
		} else {
			ResponseSet(kLine[7], code);
			ResponseSet(kLine[8], status);
		}
	}
}

void wHttpTask::Write(const std::string& body) {
	ResponseSet(kLine[9], body);
}

int wHttpTask::HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout) {
	ResponseSet(kLine[0], kMethod[0]);
	ResponseSet(kLine[1], url);
    ResponseSet(kHeader[2], Socket()->Host() + ":" + logging::NumberToString(static_cast<uint64_t>(Socket()->Port())));

    ssize_t size;
    int ret = SyncRequest(&size);
    if (ret == -1) {
    	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::HttpGet SyncRequest() failed", "");
    	return -1;
    }
    memset(mTempBuff, 0, sizeof(mTempBuff));
    
    ret = SyncResponse(mTempBuff, &size, timeout);
    if (ret == -1) {
    	LOG_ERROR(soft::GetLogPath(), "%s : %s", "wHttpTask::HttpGet SyncResponse() failed", "");
    	return ret;
    }
    res = mTempBuff;
    return 0;
}

int wHttpTask::HttpPost(const std::string& url, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout) {
    return 0;
}

}	// namespace hnet
