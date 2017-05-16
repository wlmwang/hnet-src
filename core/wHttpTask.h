
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#ifndef _W_HTTP_TASK_H_
#define _W_HTTP_TASK_H_

#include <map>
#include "wCore.h"
#include "wCommand.h"
#include "wTask.h"

namespace hnet {

const char	kCmd[][8]		= {"cmd", "para"};
const char	kProtocol[][16]	= {"HTTP/1.1", "http://"};
const char	kLine[][16]		= {"Method", "Url", "Schema", "PathInfo", "QueryString", "Get", "Post", "Code", "Status", "Body"};
const char  kMethod[][8]	= {"GET", "POST", "PUT", "DELETE", "HEAD", "PATCH"};
const char  kHeader[][32]	= {"Content-Length", "Content-Type", "Host", "Connection", "X-Powered-By", "Cache-Control", "Pragma", "Keep-Alive", "User-Agent", "Accept", "Accept-Encoding", "Accept-Language", "Accept-Charset", "Referer"};
const char	kColon[]		= ": ";
const char	kEndl[]			= "\r\n\r\n";

class wSocket;

class wHttpTask : public wTask {
public:
    wHttpTask(wSocket *socket, int32_t type = 0) : wTask(socket, type) { }

    virtual const wStatus& TaskRecv(ssize_t *size);
    virtual const wStatus& Handlemsg(char buf[], uint32_t len);

    inline std::map<std::string, std::string>& Req() { return mReq;}
    inline std::map<std::string, std::string>& Res() { return mRes;}
    
    inline std::string Url() { return kProtocol[1] + RequestGet(kHeader[2]) + mReq[kLine[1]];}
    inline std::string Method() { return mReq[kLine[0]];}
    inline std::string Pathinfo() { return mReq[kLine[3]];}
    inline std::string QueryString() { return mReq[kLine[4]];}
    inline std::string QueryGet(const std::string& key) { return mGet[key];}
    inline std::string FormGet(const std::string& key) { return mPost[key];}
    inline std::string RequestGet(const std::string& key) { return mReq[key];}

    void ResponseSet(const std::string& key, const std::string& value) { mRes[key] = value;}
    void Error(const std::string& status, const std::string& code);
    void Write(const std::string& body);

    virtual const wStatus& HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);
    virtual const wStatus& HttpPost(const std::string& url, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);

protected:
	std::map<std::string, std::string> mReq;
	std::map<std::string, std::string> mRes;

private:
    const wStatus& AsyncRequest();  // 异步接受请求
    const wStatus& AsyncResponse(); // 异步发送响应
    const wStatus& SyncRequest(ssize_t* size);  // 同步发送请求
    const wStatus& SyncResponse(char buf[], ssize_t* size, uint32_t timeout = 30);  // 同步接受响应

	const wStatus& ParseRequest(char buf[], uint32_t len);
    const wStatus& ParseResponse(char buf[], uint32_t len);
    void FillResponse();

	std::map<std::string, std::string> mGet;
	std::map<std::string, std::string> mPost;
};

}	// namespace hnet

#endif
