
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wTask.h"
#include "wCommand.h"
#include "wMisc.h"
#include "wSocket.h"
#include "wMaster.h"
#include "wWorker.h"

namespace hnet {

wTask::wTask(wSocket* socket, int32_t type) : mType(type), mSocket(socket), mHeartbeat(0), mServer(NULL), mClient(NULL), mSCType(0) {
	ResetBuffer();
}

void wTask::ResetBuffer() {
	mRecvLen = mSendLen = 0;
	mRecvRead = mRecvWrite = mRecvBuff;
	mSendRead = mSendWrite = mSendBuff;
}

wTask::~wTask() {
    SAFE_DELETE(mSocket);
}

const wStatus& wTask::HeartbeatSend() {
    mHeartbeat++;
    struct wCommand cmd;
    return AsyncSend(reinterpret_cast<char *>(&cmd), sizeof(cmd));
}

const wStatus& wTask::TaskRecv(ssize_t *size) {
	const char *buffend = mRecvBuff + kPackageSize;
	// 循环队列
	if (mRecvLen < kPackageSize && mRecvLen == 0) {
		mRecvRead = mRecvWrite = mRecvBuff;
	} else if (mRecvLen < kPackageSize && mRecvRead != mRecvBuff && mRecvWrite == buffend) {
		mRecvWrite = mRecvBuff;
	}

    // len >= 0 正向剩余
    // len < 0 分段，中间剩余
    ssize_t len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
    size_t leftlen = len>=0? static_cast<size_t>(buffend - mRecvWrite): static_cast<size_t>(abs(len));

    // socket接受数据
    *size = 0;
    if (leftlen != 0 && mRecvLen < kPackageSize) {
        if (!(mStatus = mSocket->RecvBytes(mRecvWrite, leftlen, size)).Ok() || *size < 0) {
            return mStatus;
        }
        mRecvLen += *size;
        mRecvWrite += *size;
    }

    // 消息解析
    auto handleFunc = [this] (char* buf, uint32_t len, char* nextbuf) {
        this->mStatus = this->Handlemsg(buf, len);
        this->mRecvRead = nextbuf;
        this->mRecvLen -= (len + sizeof(uint32_t));
    };

    while (mRecvLen > sizeof(uint32_t)) {
    	// 循环队列
    	if (mRecvRead == buffend && mRecvWrite != mRecvBuff) {
            mRecvRead = mRecvBuff;
        }
    	ssize_t len =  static_cast<ssize_t>(mRecvWrite - mRecvRead);
        
        uint32_t reallen = 0;
        if (len > 0) {
            // 消息字符在正向缓冲中
            reallen = coding::DecodeFixed32(mRecvRead);
            if (reallen < kMinPackageSize || reallen > kMaxPackageSize) {
                mStatus = wStatus::Corruption("wTask::TaskRecv, message length error, out range", ">0");
                break;
            } else if (reallen > static_cast<uint32_t>(len - sizeof(uint32_t))) {
            	wStatus::Corruption("wTask::TaskRecv, recv a part of message", ">0");
                mStatus.Clear();
                break;
            }

            handleFunc(mRecvRead + sizeof(uint32_t), reallen, mRecvRead + sizeof(uint32_t) + reallen);
            if (!mStatus.Ok()) {
                break;
            }
        } else {
            leftlen = buffend - mRecvRead;
            // 消息字符分段存储
            if (leftlen >= sizeof(uint32_t)) {
                reallen = coding::DecodeFixed32(mRecvRead);
            } else {
                // 长度分段
                [=, &reallen]() {
                    for (size_t pos = 0; pos < leftlen; pos++) {
                        reallen |= (static_cast<uint32_t>(static_cast<unsigned char>(mRecvRead[pos])) << (8 * pos));
                    }
                    for (size_t pos = 0; pos < sizeof(uint32_t) - leftlen; pos++) {
                        reallen |= (static_cast<uint32_t>(static_cast<unsigned char>(mRecvBuff[pos])) << (8 * pos));
                    }
                } ();
            }

            if (reallen < kMinPackageSize || reallen > kMaxPackageSize) {
                mStatus = wStatus::Corruption("wTask::TaskRecv, message length error, out range", "<=0");
                break;
            } else if (reallen > static_cast<uint32_t>(kPackageSize - abs(len) - sizeof(uint32_t))) {
            	wStatus::Corruption("wTask::TaskRecv, recv a part of message", "<=0");
                mStatus.Clear();
                break;
            }
            
            if (reallen <= static_cast<uint32_t>(leftlen - sizeof(uint32_t))) {
                // 正向剩余
                handleFunc(mRecvRead + sizeof(uint32_t), reallen, mRecvRead + sizeof(uint32_t) + reallen);
                if (!mStatus.Ok()) {
                    break;
                }
            } else {
                memcpy(mTempBuff, mRecvRead, leftlen);
                memcpy(mTempBuff + leftlen, mRecvBuff, reallen - leftlen + sizeof(uint32_t));
                handleFunc(mTempBuff + sizeof(uint32_t), reallen, mRecvBuff + (reallen - leftlen + sizeof(uint32_t)));
                if (!mStatus.Ok()) {
                    break;
                }
            }
        }
    }
    
    return mStatus;
}

const wStatus& wTask::TaskSend(ssize_t *size) {
    ssize_t len;
    const char *buffend = mSendBuff + kPackageSize;
    while (true) {
        len = mSendWrite - mSendRead;
        if (len == 0 && mSendLen == 0) {
        	mStatus.Clear();
            break;
        } else if (len > 0) {
            // len == mSendLen
            if (!(mStatus = mSocket->SendBytes(mSendRead, mSendLen, size)).Ok() || *size < 0) {
                break;
            }
            mSendLen -= *size;
            mSendRead += *size;
        } else if (len <= 0) {
            ssize_t leftlen = buffend - mSendRead;
            memcpy(mTempBuff, mSendRead, leftlen);
            memcpy(mTempBuff + leftlen, mSendBuff, mSendLen - leftlen);
            if (!(mStatus = mSocket->SendBytes(mTempBuff, mSendLen, size)).Ok() || *size < 0) {
                break;
            }
            if (*size <= leftlen) {
            	mSendRead += *size;
            } else {
            	mSendRead = mSendBuff + mSendLen - leftlen;
            }
            mSendLen -= *size;
        }
    }
    return mStatus;
}

// 整理protobuf消息至buf
void wTask::Assertbuf(char buf[], const google::protobuf::Message* msg) {
	// 类名 && 长度
	const std::string& pbName = msg->GetTypeName();
	uint16_t nameLen = static_cast<uint16_t>(pbName.size());
	// 消息体总长度
	uint32_t len = sizeof(uint8_t) + sizeof(uint16_t) + nameLen + msg->ByteSize();

	// 消息长度
	coding::EncodeFixed32(buf, len);
    // protobuf消息类型
	coding::EncodeFixed8(buf + sizeof(uint32_t), static_cast<uint8_t>(kMpProtobuf));
	// 类名长度
	coding::EncodeFixed16(buf + sizeof(uint32_t) + sizeof(uint8_t), nameLen);
	// 类名
	memcpy(buf + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t), pbName.data(), nameLen);
    // 消息体
	msg->SerializeToArray(buf + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t) + nameLen, msg->ByteSize());
}

// 整理wCommand消息至buf
void wTask::Assertbuf(char buf[], const char cmd[], size_t len) {
	// 消息长度
	coding::EncodeFixed32(buf, static_cast<uint32_t>(len + sizeof(uint8_t)));
    // wCommand消息类型
	coding::EncodeFixed8(buf + sizeof(uint32_t), static_cast<uint8_t>(kMpCommand));
	// 消息体
	memcpy(buf + sizeof(uint32_t) + sizeof(uint8_t), cmd, len);
}

const wStatus& wTask::Send2Buf(char cmd[], size_t len) {
	// 消息体总长度
	len += sizeof(uint8_t);
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::Corruption("wTask::Send2Buf, command message length error", "message too large");
    } else if (len > static_cast<size_t>(kPackageSize - mSendLen - sizeof(uint32_t))) {
        return mStatus = wStatus::Corruption("wTask::Send2Buf, command message length error", "left buffer not enough");
    }
    const char *buffend = mSendBuff + kPackageSize;
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    if ((writelen >= 0 && leftlen >= static_cast<ssize_t>(len + sizeof(uint32_t))) || (writelen < 0 && abs(writelen) >= static_cast<ssize_t>(len + sizeof(uint32_t)))) {
    	// 单向剩余足够（右边剩余 || 中间剩余）
    	Assertbuf(mSendWrite, cmd, len - sizeof(uint8_t));
    	mSendWrite += sizeof(uint32_t) + len;
    } else if (writelen >= 0 && leftlen < static_cast<ssize_t>(len + sizeof(uint32_t))) {
    	// 分段剩余足够（两边剩余）
    	Assertbuf(mTempBuff, cmd, len - sizeof(uint8_t));
    	memcpy(mSendWrite, mTempBuff, leftlen);
    	mSendWrite = mSendBuff;
    	memcpy(mSendWrite, mTempBuff + leftlen, sizeof(uint32_t) + len - leftlen);
    	mSendWrite += sizeof(uint32_t)+ len - leftlen;
    } else {
    	return mStatus = wStatus::Corruption("wTask::Send2Buf, message length error", "left buffer not enough");
    }

    mSendLen += sizeof(uint32_t) + len;
    return mStatus.Clear();
}

const wStatus& wTask::Send2Buf(const google::protobuf::Message* msg) {
	// 消息体总长度
	uint32_t len = sizeof(uint8_t) + sizeof(uint16_t) + msg->GetTypeName().size() + msg->ByteSize();
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::Corruption("wTask::Send2Buf, google::protobuf::Message length error", "message too large");
    } else if (len > static_cast<uint32_t>(kPackageSize - mSendLen - sizeof(uint32_t))) {
        return mStatus = wStatus::Corruption("wTask::Send2Buf, google::protobuf::Message length error", "left buffer not enough");
    }
    const char *buffend = mSendBuff + kPackageSize;
    ssize_t writelen =  mSendWrite - mSendRead;
    ssize_t leftlen = buffend - mSendWrite;
    if ((writelen >= 0 && leftlen >= static_cast<ssize_t>(len + sizeof(uint32_t))) || (writelen < 0 && abs(writelen) >= static_cast<ssize_t>(len + sizeof(uint32_t)))) {
    	// 单向剩余足够（右边剩余 || 中间剩余）
    	Assertbuf(mSendWrite, msg);
    	mSendWrite += sizeof(uint32_t) + len;
    } else if (writelen >= 0 && leftlen < static_cast<ssize_t>(len + sizeof(uint32_t))) {
    	// 分段剩余足够（两边剩余）
    	Assertbuf(mTempBuff, msg);
    	memcpy(mSendWrite, mTempBuff, leftlen);
    	memcpy(mSendWrite = mSendBuff, mTempBuff + leftlen, sizeof(uint32_t) + len - leftlen);
    	mSendWrite += sizeof(uint32_t)+ len - leftlen;
    } else {
    	return mStatus = wStatus::Corruption("wTask::Send2Buf, message length error", "left buffer not enough");
    }

    mSendLen += sizeof(uint32_t) + len;
    return mStatus.Clear();
}

const wStatus& wTask::SyncSend(char cmd[], size_t len, ssize_t *size) {
	// 消息体总长度
	len += sizeof(uint8_t);
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::Corruption("wTask::SyncSend, command message length error", "out range");
    }
    Assertbuf(mTempBuff, cmd, len - sizeof(uint8_t));
    return mStatus = mSocket->SendBytes(mTempBuff, len + sizeof(uint32_t), size);
}

const wStatus& wTask::SyncSend(const google::protobuf::Message* msg, ssize_t *size) {
	// 消息体总长度
	uint32_t len = sizeof(uint8_t) + sizeof(uint16_t) + msg->GetTypeName().size() + msg->ByteSize();
	if (len < kMinPackageSize || len > kMaxPackageSize) {
        return mStatus = wStatus::Corruption("wTask::SyncSend, google::protobuf::Message length error", "out range");
    }
	Assertbuf(mTempBuff, msg);
    return mStatus = mSocket->SendBytes(mTempBuff, len + sizeof(uint32_t), size);
}

const wStatus& wTask::AsyncSend(char cmd[], size_t len) {
	if (mSCType == 0 && mServer != NULL) {
		mStatus = mServer->Send(this, cmd, len);
	} else if (mSCType == 1 && mClient != NULL) {
		mStatus = mClient->Send(this, cmd, len);
	} else {
		mStatus = wStatus::Corruption("wTask::AsyncSend, failed", "mServer or mClient is null");
	}
	return mStatus;
}

const wStatus& wTask::AsyncSend(const google::protobuf::Message* msg) {
	if (mSCType == 0 && mServer != NULL) {
		mStatus = mServer->Send(this, msg);
	} else if (mSCType == 1 && mClient != NULL) {
		mStatus = mClient->Send(this, msg);
	} else {
		mStatus = wStatus::Corruption("wTask::AsyncSend, failed", "mServer or mClient is null");
	}
	return mStatus;
}

const wStatus& wTask::SyncRecv(char cmd[], ssize_t *size, uint32_t timeout) {
	size_t headlen = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t);
	size_t recvheadlen = 0, recvbodylen = 0;
	for (uint64_t step = 1, usc = 1, timeline = timeout * 1000000; usc <= timeline; step <<= 1, usc += step) {
		// 头信息
        if (!(mStatus = mSocket->RecvBytes(mTempBuff + recvheadlen, headlen - recvheadlen, size)).Ok()) {
            return mStatus;
        } else if (*size != -1) {
        	recvheadlen += *size;
        }
        if (recvheadlen != headlen) {
        	usleep(step);
        	continue;
        }

        // 忽略心跳包干扰
        struct wCommand* nullcmd = reinterpret_cast<struct wCommand*>(mTempBuff + sizeof(uint32_t) + sizeof(uint8_t));
        if (nullcmd->GetId() == CmdId(kCmdNull, kParaNull)) {
        	recvheadlen = 0;
            continue;
        }

        // 接受消息体
        uint32_t reallen = static_cast<size_t>(coding::DecodeFixed32(mTempBuff) - sizeof(uint8_t) - sizeof(uint16_t));
        timeline -= usc;
        for (step = 1; usc <= timeline; step <<= 1, usc += step) {
            if (!(mStatus = mSocket->RecvBytes(mTempBuff + recvheadlen + recvbodylen, reallen - recvbodylen, size)).Ok()) {
                return mStatus;
            }
            recvbodylen += *size;
            if (recvbodylen == reallen) {
            	break;
            } else {
            	usleep(step);
            	continue;
            }
        }
        break;
	}

    uint32_t msglen = coding::DecodeFixed32(mTempBuff);
    if (msglen < kMinPackageSize || msglen > kMaxPackageSize) {
        return mStatus = wStatus::Corruption("wTask::SyncRecv, message length error", "out range");
    } else if (msglen + sizeof(uint32_t) != recvheadlen + recvbodylen) {
    	return mStatus = wStatus::Corruption("wTask::SyncRecv, message length error", "illegal message");
    }

    *size = msglen - sizeof(uint8_t);
    memcpy(cmd, mTempBuff + sizeof(uint32_t) + sizeof(uint8_t), *size);
    return mStatus.Clear();
}

const wStatus& wTask::SyncRecv(google::protobuf::Message* msg, ssize_t *size, uint32_t timeout) {
	size_t headlen = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t);
	size_t recvheadlen = 0, recvbodylen = 0;
	for (uint64_t step = 1, usc = 1, timeline = timeout * 1000000; usc <= timeline; step <<= 1, usc += step) {
		// 头信息
        if (!(mStatus = mSocket->RecvBytes(mTempBuff + recvheadlen, headlen - recvheadlen, size)).Ok()) {
            return mStatus;
        } else if (*size != -1) {
        	recvheadlen += *size;
        }
        if (recvheadlen != headlen) {
        	usleep(step);
        	continue;
        }

        // 忽略心跳包干扰
        struct wCommand* nullcmd = reinterpret_cast<struct wCommand*>(mTempBuff + sizeof(uint32_t) + sizeof(uint8_t));
        if (nullcmd->GetId() == CmdId(kCmdNull, kParaNull)) {
        	recvheadlen = 0;
            continue;
        }

        // 接受消息体
        uint32_t reallen = static_cast<size_t>(coding::DecodeFixed32(mTempBuff) - sizeof(uint8_t) - sizeof(uint16_t));
        timeline -= usc;
        for (step = 1; usc <= timeline; step <<= 1, usc += step) {
            if (!(mStatus = mSocket->RecvBytes(mTempBuff + recvheadlen + recvbodylen, reallen - recvbodylen, size)).Ok()) {
                return mStatus;
            }
            recvbodylen += *size;
            if (recvbodylen == reallen) {
            	break;
            } else {
            	usleep(step);
            	continue;
            }
        }
        break;
	}

    uint32_t msglen = coding::DecodeFixed32(mTempBuff);
    if (msglen < kMinPackageSize || msglen > kMaxPackageSize) {
        return mStatus = wStatus::Corruption("wTask::SyncRecv, message length error", "out range");
    } else if (msglen + sizeof(uint32_t) != recvheadlen + recvbodylen) {
    	return mStatus = wStatus::Corruption("wTask::SyncRecv, message length error", "illegal message");
    }

    uint16_t n = coding::DecodeFixed16(mTempBuff + sizeof(uint32_t) + sizeof(uint8_t));
    msg->ParseFromArray(mTempBuff + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint16_t) + n, msglen - sizeof(uint8_t) - sizeof(uint16_t) - n);
    return mStatus.Clear();
}

const wStatus& wTask::SyncWorker(char cmd[], size_t len) {
	std::vector<uint32_t> blacksolt(1, mServer->Worker()->Slot());
	return mStatus = mServer->NotifyWorker(cmd, len, kMaxProcess, &blacksolt);
}

const wStatus& wTask::SyncWorker(const google::protobuf::Message* msg) {
	std::vector<uint32_t> blacksolt(1, mServer->Worker()->Slot());
	return mStatus = mServer->NotifyWorker(msg, kMaxProcess, &blacksolt);
}

const wStatus& wTask::Handlemsg(char cmd[], uint32_t len) {
	// 数据协议
	uint8_t sp = static_cast<uint8_t>(coding::DecodeFixed8(cmd));
	cmd += sizeof(uint8_t);
	len -= sizeof(uint8_t);

	if (sp == kMpCommand) {
		struct wCommand *basecmd = reinterpret_cast<struct wCommand*>(cmd);
		if (basecmd->GetId() == CmdId(kCmdNull, kParaNull)) {
			mHeartbeat = 0;
			mStatus.Clear();
		} else {
			struct Request_t request(cmd, len);
			if (mEventCmd(basecmd->GetId(), &request) == false) {
				mStatus = wStatus::Corruption("wTask::Handlemsg, command invalid request", "no method find");
			}
		}
	} else if (sp == kMpProtobuf) {
		uint16_t l = coding::DecodeFixed16(cmd);
		std::string name(cmd + sizeof(uint16_t), l);
		struct Request_t request(cmd + sizeof(uint16_t) + l, len - sizeof(uint16_t) - l);
		if (mEventPb(name, &request) == false) {
			mStatus = wStatus::Corruption("wTask::Handlemsg, protobuf invalid request", "no method find");
		}
	} else {
		mStatus = wStatus::Corruption("wTask::Handlemsg, invalid message protocol", logging::NumberToString(sp));
	}
	return mStatus;
}

}   // namespace hnet
