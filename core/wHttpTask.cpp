
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
					memset(mTempBuff, 0, sizeof(mTempBuff));
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
				// POST请求
				memset(mTempBuff, 0, sizeof(mTempBuff));
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
			} else {
				// 未知请求
				mStatus = wStatus::Corruption("wHttpTask::TaskRecv, method error", "<=0");
				break;
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
			ResponseSet(kHeader[3], "close");
			Error("Not Found(cmd,para illegal)", "404");
		}
	} else {
		ResponseSet(kHeader[3], "close");
		Error("Bad Request(cmd,para must)", "400");
	}
	return AsyncResponse();
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
	return mStatus;
}

const wStatus& wHttpTask::AsyncResponse() {
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
    	return mStatus = wStatus::Corruption("wHttpTask::ParseResponse, message length error", "left buffer not enough");
    }
    mSendLen += len;
	return mStatus = Output();
}

const wStatus& wHttpTask::SyncResponse(char buf[], ssize_t* size, uint32_t timeout) {
	int32_t pos = 0, pos1 = 0, len = 0;
	size_t recvlen = 0;
	bool ok = false;
	memset(mTempBuff, 0, sizeof(mTempBuff));
	for (uint64_t step = 1, usc = 1, timeline = timeout * 1000000; usc <= timeline; step <<= 1, usc += step) {
		if (!(mStatus = mSocket->RecvBytes(mTempBuff + recvlen, kPackageSize, size)).Ok()) {
			return mStatus;
        } else if (*size > 0) {
        	recvlen += *size;
        }
        if (recvlen < strlen(kProtocol[0])) {
   			usleep(step);
            continue;
        }

        if (misc::Strcmp(std::string(mTempBuff, 0, strlen(kProtocol[0])), kProtocol[0], strlen(kProtocol[0])) == 0) {
       		pos = misc::Strpos(std::string(mTempBuff, 0, recvlen), kEndl);
       		if (pos == -1) {
	   			usleep(step);
	            continue;
       		}
	   		pos1 = misc::Strpos(std::string(mTempBuff, 0, recvlen), kHeader[0]);
	   		if (pos1 == -1) {
	   			usleep(step);
	   			continue;
	   		}

	   		len = atoi(mTempBuff + pos1 + strlen(kHeader[0]) + strlen(kColon));
	   		if (pos + strlen(kEndl) + len > kMaxPackageSize) {
	   			mStatus = wStatus::Corruption("wHttpTask::SyncResponse, header error(Content-Length out range)", "");
	   			break;
	   		} else if (pos + strlen(kEndl) + len > static_cast<uint32_t>(recvlen)) {
	   			usleep(step);
	   			continue;
	   		} else {
	   			ok = true;
	   		}
        } else {
       		// 未知协议
            mStatus = wStatus::Corruption("wHttpTask::SyncResponse, only support HTTP/1.1", "");
            break;
        }
        break;
	}

	if (ok == false) {
		return mStatus.Ok()? (mStatus = wStatus::Corruption("wTask::SyncResponse, message length error", "illegal message")): mStatus;
	}
	*size = recvlen;
	memcpy(buf, mTempBuff , *size);
    return mStatus.Clear();
}

const wStatus& wHttpTask::AsyncRequest() {
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
    	return mStatus = wStatus::Corruption("wHttpTask::ParseResponse, message length error", "left buffer not enough");
    }
    mSendLen += len;
    return mStatus = Output();
}

const wStatus& wHttpTask::SyncRequest(ssize_t* size) {
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

const wStatus& wHttpTask::HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout) {
	ResponseSet(kLine[0], kMethod[0]);
	ResponseSet(kLine[1], url);
    ResponseSet(kHeader[2], Socket()->Host() + ":" + logging::NumberToString(static_cast<uint64_t>(Socket()->Port())));

    ssize_t size;
    if (!(mStatus = SyncRequest(&size)).Ok()) {
    	return mStatus;
    }
    memset(mTempBuff, 0, sizeof(mTempBuff));
    if (!(mStatus = SyncResponse(mTempBuff, &size, timeout)).Ok()) {
    	return mStatus;
    }
    res = mTempBuff;
    return mStatus;
}

const wStatus& wHttpTask::HttpPost(const std::string& url, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout) {
    return mStatus;
}

}	// namespace hnet
