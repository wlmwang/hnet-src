
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wTask.h"
#include "wCommand.h"
#include "wMisc.h"
#include "wLogger.h"
#include "wSocket.h"
#include "wMaster.h"
#include "wWorker.h"

namespace hnet {

wTask::wTask(wSocket* socket, int32_t type) : mType(type), mSocket(socket), mHeartbeat(0), mServer(NULL), mClient(NULL), mSCType(-1) {
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

int wTask::HeartbeatSend() {
    mHeartbeat++;
    struct wCommand cmd;
    return AsyncSend(reinterpret_cast<char*>(&cmd), sizeof(cmd));
}

int wTask::Output() {
    if (mSCType == 0 && mServer) {
        if (mServer->AddTask(this, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Output AddTask() failed", "");
            return -1;
        }
    } else if (mSCType == 1 && mClient) {
        if (mClient->AddTask(this, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD, false) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Output AddTask() failed", "");
            return -1;
        }
    } else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Output () failed", "mServer or mClient is null");
        return -1;
    }
    return 0;
}

int wTask::TaskRecv(ssize_t *size) {
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
    	if (len >= 0 && leftlen <= 4*sizeof(uint32_t)) {
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
            if (ret == -1 || *size < 0) {
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
        this->mRecvLen -= len + sizeof(uint32_t);
        return ret;
    };

    int ret = 0;
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
                ret = -1;
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::TaskRecv () failed", ">0 message length error");
                break;

            } else if (reallen > static_cast<uint32_t>(len - sizeof(uint32_t))) {
                ret = 0;
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::TaskRecv () failed", ">0 recv a part of message");
                break;
            }

            ret = handleFunc(mRecvRead + sizeof(uint32_t), reallen, mRecvRead + sizeof(uint32_t) + reallen);
            if (ret == -1) {
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
                ret = -1;
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::TaskRecv () failed", "<=0 message length error");
                break;

            } else if (reallen > static_cast<uint32_t>(kPackageSize - abs(len) - sizeof(uint32_t))) {
                ret = 0;
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::TaskRecv () failed", "<=0 recv a part of message");
                break;
            }
            
            if (reallen <= static_cast<uint32_t>(leftlen - sizeof(uint32_t))) { // 正向剩余
                ret = handleFunc(mRecvRead + sizeof(uint32_t), reallen, mRecvRead + sizeof(uint32_t) + reallen);
                if (ret == -1) {
                    break;
                }
            } else {
                memcpy(mTempBuff, mRecvRead, leftlen);
                memcpy(mTempBuff + leftlen, mRecvBuff, reallen - leftlen + sizeof(uint32_t));
                
                ret = handleFunc(mTempBuff + sizeof(uint32_t), reallen, mRecvBuff + (reallen - leftlen + sizeof(uint32_t)));
                if (ret == -1) {
                    break;
                }
            }
        }
    }
    return ret;
}

int wTask::TaskSend(ssize_t *size) {
    ssize_t len;
    const char *buffend = mSendBuff + kPackageSize;
    int ret = 0;
    while (true) {
        len = mSendWrite - mSendRead;
        if (len == 0 && mSendLen == 0) {
            break;
        } else if (len > 0) {
            ret = mSocket->SendBytes(mSendRead, mSendLen, size);
            if (ret == -1 || *size < 0) {
                break;
            }

            mSendLen -= *size;
            mSendRead += *size;
        } else if (len <= 0) {
            ssize_t leftlen = buffend - mSendRead;
            memcpy(mTempBuff, mSendRead, leftlen);
            memcpy(mTempBuff + leftlen, mSendBuff, mSendLen - leftlen);
            ret = mSocket->SendBytes(mTempBuff, mSendLen, size);
            if (ret == -1 || *size < 0) {
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
    return ret;
}

#ifdef _USE_PROTOBUF_
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
	msg->SerializeToArray(reinterpret_cast<void*>(buf+sizeof(uint32_t)+sizeof(uint8_t)+sizeof(uint16_t)+nameLen), msg->ByteSize());
}
#endif

// 整理wCommand消息至buf
void wTask::Assertbuf(char buf[], const char cmd[], size_t len) {
	// 消息长度
	coding::EncodeFixed32(buf, static_cast<uint32_t>(len + sizeof(uint8_t)));
    // wCommand消息类型
	coding::EncodeFixed8(buf + sizeof(uint32_t), static_cast<uint8_t>(kMpCommand));
	// 消息体
	memcpy(buf + sizeof(uint32_t) + sizeof(uint8_t), cmd, len);
}

int wTask::Send2Buf(char cmd[], size_t len) {
	// 消息体总长度
	len += sizeof(uint8_t);
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Send2Buf () failed", "message too large");
        return -1;

    } else if (len > static_cast<size_t>(kPackageSize - mSendLen - sizeof(uint32_t))) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Send2Buf () failed", "left buffer not enough");
        return -1;
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
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Send2Buf () failed", "left buffer not enough");
        return -1;
    }
    mSendLen += sizeof(uint32_t) + len;
    return 0;
}

#ifdef _USE_PROTOBUF_
int wTask::Send2Buf(const google::protobuf::Message* msg) {
	// 消息体总长度
	uint32_t len = sizeof(uint8_t) + sizeof(uint16_t) + msg->GetTypeName().size() + msg->ByteSize();
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Send2Buf () failed", "message too large");
        return -1;

    } else if (len > static_cast<uint32_t>(kPackageSize - mSendLen - sizeof(uint32_t))) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Send2Buf () failed", "left buffer not enough");
        return -1;
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
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::Send2Buf () failed", "left buffer not enough");
        return -1;
    }

    mSendLen += sizeof(uint32_t) + len;
    return 0;
}
#endif

int wTask::SyncWorker(char cmd[], size_t len) {
    if (mServer && mServer->Worker()) {
        std::vector<uint32_t> blackslot(1, mServer->Worker()->Slot());
        if (mServer->SyncWorker(cmd, len, kMaxProcess, &blackslot) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncWorker SyncWorker() failed", "");
            return -1;
        }
    } else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncWorker () failed", "server is null");
        return -1;
    }
    return 0;
}

int wTask::AsyncWorker(char cmd[], size_t len) {
    if (mServer && mServer->Worker()) {
        std::vector<uint32_t> blackslot(1, mServer->Worker()->Slot());
        if (mServer->AsyncWorker(cmd, len, kMaxProcess, &blackslot) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncWorker AsyncWorker() failed", "");
            return -1;
        }
    } else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncWorker () failed", "server is null");
        return -1;
    }
    return 0;
}

#ifdef _USE_PROTOBUF_
int wTask::SyncWorker(const google::protobuf::Message* msg) {
    if (mServer && mServer->Worker()) {
        std::vector<uint32_t> blackslot(1, mServer->Worker()->Slot());
        if (mServer->SyncWorker(msg, kMaxProcess, &blackslot) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncWorker SyncWorker() failed", "");
            return -1;
        }
    } else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncWorker () failed", "server is null");
        return -1;
    }
    return 0;
}
#endif

#ifdef _USE_PROTOBUF_
int wTask::AsyncWorker(const google::protobuf::Message* msg) {
    if (mServer && mServer->Worker()) {
        std::vector<uint32_t> blackslot(1, mServer->Worker()->Slot());
        if (mServer->AsyncWorker(msg, kMaxProcess, &blackslot) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncWorker SyncWorker() failed", "");
            return -1;
        }
    } else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncWorker () failed", "server is null");
        return -1;
    }
    return 0;
}
#endif

int wTask::AsyncSend(char cmd[], size_t len) {
	if (mSCType == 0 && mServer != NULL) {
        if (mServer->Send(this, cmd, len) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncSend Send() failed", "");
            return -1;
        }
	} else if (mSCType == 1 && mClient != NULL) {
        if (mClient->Send(this, cmd, len) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncSend Send() failed", "");
            return -1;
        }
	} else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncSend () failed", "server or client is null");
        return -1;
	}
	return 0;
}

#ifdef _USE_PROTOBUF_
int wTask::AsyncSend(const google::protobuf::Message* msg) {
	if (mSCType == 0 && mServer != NULL) {
        if (mServer->Send(this, msg) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncSend Send() failed", "");
            return -1;
        }
	} else if (mSCType == 1 && mClient != NULL) {
        if (mClient->Send(this, msg) == -1) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncSend Send() failed", "");
            return -1;
        }
	} else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::AsyncSend () failed", "server or client is null");
        return -1;
	}
	return 0;
}
#endif

int wTask::SyncSend(char cmd[], size_t len, ssize_t *size) {
	// 消息体总长度
	len += sizeof(uint8_t);
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncSend () failed", "message length error");
        return -1;
    }

    Assertbuf(mTempBuff, cmd, len - sizeof(uint8_t));
    return mSocket->SendBytes(mTempBuff, len + sizeof(uint32_t), size);
}

#ifdef _USE_PROTOBUF_
int wTask::SyncSend(const google::protobuf::Message* msg, ssize_t *size) {
	// 消息体总长度
	uint32_t len = sizeof(uint8_t) + sizeof(uint16_t) + msg->GetTypeName().size() + msg->ByteSize();
	if (len < kMinPackageSize || len > kMaxPackageSize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncSend () failed", "message length error");
        return -1;
    }

	Assertbuf(mTempBuff, msg);
    return mSocket->SendBytes(mTempBuff, len + sizeof(uint32_t), size);
}
#endif

int wTask::SyncRecv(char cmd[], ssize_t *size, size_t msglen, uint32_t timeout) {
    static const size_t kCmdHeadLen = sizeof(uint32_t) + sizeof(uint8_t); // 包长度 + 数据协议

	size_t recvheadlen = 0, recvbodylen = 0, headlen = 0;
    uint64_t now = soft::TimeUsec();
    if (msglen > 0) {
        headlen = kCmdHeadLen + msglen;
    } else {
        headlen = kCmdHeadLen + sizeof(uint16_t);  // 至少有一条消息
    }

    while (true) {
        // 超时时间设置
        if (timeout > 0) {
            double tmleft = static_cast<int64_t>(timeout*1000000) - (soft::TimeUpdate() - now);
            if (tmleft > 0) {
                if (mSocket->SetRecvTimeout(tmleft/1000000) == -1) {
                    return -1;
                }
            } else {
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "timeout");
                return -1;
            }
        }

        // 接受消息
        int ret = mSocket->RecvBytes(mTempBuff + recvheadlen, headlen - recvheadlen, size);
        if (ret == -1) {
            return -1;
        }

        if (*size > 0) {
            recvheadlen += *size;
        }
        if (recvheadlen < headlen) {
            continue;
        }

        // 忽略心跳包干扰
        struct wCommand* nullcmd = reinterpret_cast<struct wCommand*>(mTempBuff + kCmdHeadLen);
        if (nullcmd->GetId() == CmdId(kCmdNull, kParaNull)) {
            recvheadlen -= kCmdHeadLen + sizeof(struct wCommand);
            memmove(mTempBuff, mTempBuff + kCmdHeadLen + sizeof(struct wCommand), recvheadlen);
            continue;
        }

        // 指定长度消息直接返回
        if (msglen > 0 || recvheadlen != headlen) {
            break;
        }

        // 接受消息体
        uint32_t reallen = static_cast<size_t>(coding::DecodeFixed32(mTempBuff) - sizeof(uint8_t) - sizeof(uint16_t));
        ret = mSocket->RecvBytes(mTempBuff + recvheadlen + recvbodylen, reallen - recvbodylen, size);
        if (ret == -1) {
            return -1;
        }

        if (*size > 0) {
            recvbodylen += *size;
        }
        break;
    }

    uint32_t len = coding::DecodeFixed32(mTempBuff);
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "message length error,out range");
        return -1;

    } else if (len + sizeof(uint32_t) != recvheadlen + recvbodylen) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "message length error,illegal message");
        return -1;
    }

    *size = len - sizeof(uint8_t);
    if (msglen > 0 && msglen != static_cast<size_t>(*size)) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "message length error,error message");
        return -1;
    }
    memcpy(cmd, mTempBuff + kCmdHeadLen, *size);
    return 0;
}

#ifdef _USE_PROTOBUF_
int wTask::SyncRecv(google::protobuf::Message* msg, ssize_t *size, size_t msglen, uint32_t timeout) {
    static const size_t kCmdHeadLen = sizeof(uint32_t) + sizeof(uint8_t); // 包长度 + 数据协议

    size_t recvheadlen = 0, recvbodylen = 0, headlen = 0;
    int64_t now = soft::TimeUsec();
    if (msglen > 0) {
        headlen = kCmdHeadLen + msglen + sizeof(uint16_t) + msg->GetTypeName().size();  // 类名协议
    } else {
        headlen = kCmdHeadLen + sizeof(uint16_t);  // 至少有一条消息
    }

    while (true) {
        // 超时时间设置
        if (timeout > 0) {
            double tmleft = static_cast<int64_t>(timeout*1000000) - (soft::TimeUpdate() - now);
            if (tmleft > 0) {
                if (mSocket->SetRecvTimeout(tmleft/1000000) == -1) {
                    return -1;
                }
            } else {
                LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "timeout");
                return -1;
            }
        }

        int ret = mSocket->RecvBytes(mTempBuff + recvheadlen, headlen - recvheadlen, size);
        if (ret == -1) {
            return -1;
        }

        if (*size > 0) {
            recvheadlen += *size;
        }
        if (recvheadlen < headlen) {
            continue;
        }

        // 忽略心跳包干扰
        struct wCommand* nullcmd = reinterpret_cast<struct wCommand*>(mTempBuff + kCmdHeadLen);
        if (nullcmd->GetId() == CmdId(kCmdNull, kParaNull)) {
            recvheadlen -= kCmdHeadLen + sizeof(struct wCommand);
            memmove(mTempBuff, mTempBuff + kCmdHeadLen + sizeof(struct wCommand), recvheadlen);
            continue;
        }

        // 指定长度消息直接返回
        if (msglen > 0 || recvheadlen != headlen) {
            break;
        }

        uint32_t reallen = static_cast<size_t>(coding::DecodeFixed32(mTempBuff) - sizeof(uint8_t) - sizeof(uint16_t));
        ret = mSocket->RecvBytes(mTempBuff + recvheadlen + recvbodylen, reallen - recvbodylen, size);
        if (ret == -1) {
            return -1;
        }

        if (*size > 0) {
            recvbodylen += *size;
        }
        break;
    }

    uint32_t len = coding::DecodeFixed32(mTempBuff);
    if (len < kMinPackageSize || len > kMaxPackageSize) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "message length error,out range");
        return -1;

    } else if (len + sizeof(uint32_t) != recvheadlen + recvbodylen) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "message length error,illegal message");
        return -1;
    }

    uint32_t n = sizeof(uint16_t) + coding::DecodeFixed16(mTempBuff + kCmdHeadLen); // 类名长度
    *size = len - sizeof(uint8_t) - n;
    if (msglen > 0 && msglen != static_cast<size_t>(*size)) {
        LOG_ERROR(soft::GetLogPath(), "%s : %s", "wTask::SyncRecv () failed", "message length error,error message");
        return -1;
    }
    msg->ParseFromArray(mTempBuff + kCmdHeadLen + n, *size);
    return 0;
}
#endif

int wTask::Handlemsg(char cmd[], uint32_t len) {
	// 数据协议
	uint8_t sp = static_cast<uint8_t>(coding::DecodeFixed8(cmd));
	cmd += sizeof(uint8_t);
	len -= sizeof(uint8_t);

    int ret = 0;
	if (sp == kMpCommand) {
		struct wCommand *basecmd = reinterpret_cast<struct wCommand*>(cmd);
		if (basecmd->GetId() == CmdId(kCmdNull, kParaNull)) {
			mHeartbeat = 0;
		} else {
			struct Request_t request(cmd, len);
			if (mEventCmd(basecmd->GetId(), &request) == false) {
				std::string id = "id:";
				logging::AppendNumberTo(&id, static_cast<uint64_t>(basecmd->GetId()));
				id += ", cmd:";
				logging::AppendNumberTo(&id, static_cast<uint64_t>(basecmd->GetCmd()));
				id += ", para:";
				logging::AppendNumberTo(&id, static_cast<uint64_t>(basecmd->GetPara()));

                LOG_ERROR(soft::GetLogPath(), "%s : %s[id=%s]", "wTask::Handlemsg () failed", "request invalid", id.c_str());
                ret = -1;
			}
		}
	} else if (sp == kMpProtobuf) {
#ifdef _USE_PROTOBUF_
		uint16_t l = coding::DecodeFixed16(cmd);
		std::string name(cmd + sizeof(uint16_t), l);
		struct Request_t request(cmd + sizeof(uint16_t) + l, len - sizeof(uint16_t) - l);
		if (mEventPb(name, &request) == false) {
            LOG_ERROR(soft::GetLogPath(), "%s : %s[name=%s]", "wTask::Handlemsg () failed", "request invalid", name.c_str());
            ret = -1;
		}
#else
        LOG_ERROR(soft::GetLogPath(), "%s : %s[sp=%d]", "wTask::Handlemsg () failed", "protobuf invalid", sp);
        ret = -1;
#endif
	} else {
        LOG_ERROR(soft::GetLogPath(), "%s : %s[sp=%d]", "wTask::Handlemsg () failed", "request invalid", sp);
        ret = -1;
	}

	return ret;
}

}   // namespace hnet
