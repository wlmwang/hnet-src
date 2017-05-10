
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
#include "wHttpStatusCode.h"

namespace hnet {

class wSocket;

const char	kLine[][16]		= {"method", "uri", "schema", "pathinfo", "querystring", "get", "post", "code", "status", "body"};
const char	kProtocol[][16]	= {"HTTP/1.1", "http://"};
const char  kMethod[][8]	= {"GET", "POST", "PUT", "DELETE", "HEAD", "PATCH"};
const char  kHeader[][32]	= {"Content-Length", "Content-Type", "Host", "Connection", "X-Powered-By", "Cache-Control", "Pragma", "Keep-Alive", 
								"User-Agent", "Accept", "Accept-Encoding", "Accept-Language", "Accept-Charset", "Referer"};
const char	kCmd[][8]		= {"cmd", "para"};
const char	kColon[]		= ": ";
const char	kEndl[]			= "\r\n\r\n";

class wHttpTask : public wTask {
public:
    wHttpTask(wSocket *socket, int32_t type = 0) : wTask(socket, type) { }

    virtual const wStatus& TaskRecv(ssize_t *size);
    virtual const wStatus& Handlemsg(char buf[], uint32_t len);

    inline std::map<std::string, std::string> Req() { return mReq;}
    inline std::map<std::string, std::string> Res() { return mRes;}
    
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

protected:
	std::map<std::string, std::string> mReq;
	std::map<std::string, std::string> mRes;

private:
	const wStatus& ParseRequest(char buf[], uint32_t len);
	const wStatus& ParseResponse();
	void AssertResponse();

	std::map<std::string, std::string> mGet;
	std::map<std::string, std::string> mPost;
};

}	// namespace hnet

#endif
