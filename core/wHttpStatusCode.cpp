
/**
 * Copyright (C) Anny Wang.
 * Copyright (C) Hupu, Inc.
 */

#include "wHttpStatusCode.h"

namespace hnet {
	
static struct HttpStatusCode_t statusCodes[] = {
	{"100",	"Continue"},
	{"101",	"Switching Protocols"},

	{"200",	"Ok"},
	{"201",	"Created"},
	{"202",	"Accepted"},
	{"203",	"Non-authoritative Information"},
	{"204",	"No Content"},
	{"205",	"Reset Content"},
	{"206",	"Partial Content"},

	{"300",	"Multiple Choices"},
	{"301",	"Moved Permanently"},
	{"302",	"Found"},
	{"303",	"See Other"},
	{"304",	"Not Modified"},
	{"305",	"Use Proxy"},
	{"306",	"Unused"},
	{"307",	"Temporary Redirect"},

	{"400",	"Bad Request"},
	{"401",	"Unauthorized"},
	{"402",	"Payment Required"},
	{"403",	"Forbidden"},
	{"404",	"Not Found"},
	{"405",	"Method Not Allowed"},
	{"406",	"Not Acceptable"},
	{"407",	"Proxy Authentication Required"},
	{"408",	"Request Timeout"},
	{"409",	"Conflict"},
	{"410",	"Gone"},
	{"411",	"Length Required"},
	{"412",	"Precondition Failed"},
	{"413",	"Request Entity Too Large"},
	{"414",	"Request-url Too Long"},
	{"415",	"Unsupported Media Type"},
	{"416",	""},
	{"417",	"Expectation Failed"},

	{"500",	"Internal Server Error"},
	{"501",	"Not Implemented"},
	{"502",	"Bad Gateway"},
	{"503",	"Service Unavailable"},
	{"504",	"Gateway Timeout"},
	{"505",	"HTTP Version Not Supported"},

	{"", ""}
};

std::map<const std::string, const std::string> gHttpStatusCode;
void HttpStatusCodeInit() {
	for (int i = 0; !statusCodes[i].code; i++) {
		gHttpStatusCode.insert(std::make_pair(statusCodes[i].code, statusCodes[i].status));
	}
}

const std::string& HttpStatus(std::string& code) {
	return gHttpStatusCode[code];
}

}	// namespace hnet