/*
 * headerValue.cpp
 *
 *  Created on: Jan 20, 2016
 *      Author: ondra
 */


#include "headerValue.h"
#include "lightspeed/base/containers/constStr.h"

#include "lightspeed/base/countof.h"

using LightSpeed::countof;
namespace BredyHttpSrv {

static ConstStrA hdrfields[] = {
		/*fldHost ,*/ "Host",
		/*fldUserAgent,*/ "User-Agent",
		/*fldServer,*/ "Server",
		/*fldContentType,*/ "Content-Type",
		/*fldContentLength,*/ "Content-Length",
		/*fldConnection,*/ "Connection",
		/*fldCookie,*/ "Cookie",
		/*fldAccept, */"Accept",
		/*fldCacheControl,*/"Cache-Control",
		/*fldDate,*/ "Date",
		/*fldReferer,*/ "Referer",
		/*fldAllow,*/ "Allow",
		/*fldContentDisposition,*/ "Content-Disposition",
		/*fldExpires,*/ "Expires",
		/*fldLastModified,*/ "Last-Modified",
		/*fldLocation,*/ "Location",
		/*fldPragma,*/ "Pragma",
		/*fldRefresh,*/ "Referer",
		/*fldSetCookie,*/ "Set-Cookie",
		/*fldWWWAuthenticate,*/ "WWW-Authenticate",
		/*fldAuthorization,*/ "Authorization",
		/*fldWarning,*/ "Warning",
		/*fldAccessControlAllowOrigin,*/ "Access-Control-Allow-Origin",
		/*fldETag,*/ "ETag",
		/*fldIfNoneMatch,*/ "If-None-Match",
		/*fldIfModifiedSince,*/ "If-Modified-Since",
		/*fldTransferEncoding,*/ "Transfer-Encoding",
		/*fldExpect,*/ "Expect",
		/*fldUnknown,*/ "Undefined",
		/*fldUpgrade,*/ "Upgrade",
		/*fldAccessControlAllowMethods,*/ "Access-Control-Allow-Methods",
		/*fldAccessControlAllowHeaders,*/ "Access-Control-Allow-Headers",
		/*fldXForwardedFor*/ "X-Forwarded-For",
		/*fldOrigin*/ "Origin"


};

ConstStrA HeaderFieldDef::getHeaderFieldName(Field fld) {
	natural idx = fld;
	if (idx >= countof(hdrfields))
		return ConstStrA();

	return hdrfields[idx];
}


}

