
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
    virtual ~wHttpTask() { }

    virtual int TaskRecv(ssize_t *size);
    virtual int Handlemsg(char buf[], uint32_t len);

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

    virtual int HttpGet(const std::string& url, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);
    virtual int HttpPost(const std::string& url, const std::map<std::string, std::string>& data, const std::map<std::string, std::string>& header, std::string& res, uint32_t timeout = 30);

protected:
	std::map<std::string, std::string> mReq;
	std::map<std::string, std::string> mRes;

private:
    int AsyncRequest();  // 异步接受请求
    int AsyncResponse(); // 异步发送响应
    
    int SyncRequest(ssize_t* size);  // 同步发送请求
    
    // 同步接受一条合法的消息（该消息必须为一条即将接受的消息）
    // 调用者：保证此sock未加入epoll中，否则出现事件竞争；且该sock需为阻塞的fd；另外也要确保buf有足够长的空间接受自此同步消息
    // size = -1 对端发生错误|稍后重试
    // size = 0  对端关闭
    // size > 0  接受字符
    int SyncResponse(char buf[], ssize_t* size, uint32_t timeout = 30);  // 同步接受响应

	int ParseRequest(char buf[], uint32_t len);
    int ParseResponse(char buf[], uint32_t len);
    void FillResponse();

	std::map<std::string, std::string> mGet;
	std::map<std::string, std::string> mPost;
};

}	// namespace hnet

#endif
