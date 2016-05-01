/*
 * headerValue.h
 *
 *  Created on: 19. 10. 2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_BREDY_HTTPSERVER_HEADERVALUE_H_
#define JSONRPCSERVER_BREDY_HTTPSERVER_HEADERVALUE_H_
#include "lightspeed/base/containers/constStr.h"

namespace BredyHttpSrv {


using namespace LightSpeed;

	class HeaderValue: public ConstStrA {
	public:
		const bool defined;
		HeaderValue():defined(false) {}
		HeaderValue(ConstStrA value):ConstStrA(value),defined(true) {}
	};

	class HeaderFieldDef {
	public:

		enum Field {
			fldHost = 0,
			fldUserAgent,
			fldServer,
			fldContentType,
			fldContentLength,
			fldConnection,
			fldCookie,
			fldAccept,
			fldCacheControl,
			fldDate,
			fldReferer,
			fldAllow,
			fldContentDisposition,
			fldExpires,
			fldLastModified,
			fldLocation,
			fldPragma,
			fldRefresh,
			fldSetCookie,
			fldWWWAuthenticate,
			fldAuthorization,
			fldWarning,
			fldAccessControlAllowOrigin,
			fldETag,
			fldIfNoneMatch,
			fldIfModifiedSince,
			fldTransferEncoding,
			fldExpect,
			fldUnknown,
			fldUpgrade,
			fldAccessControlAllowMethods,
			fldAccessControlAllowHeaders,
			fldXForwardedFor,
			fldOrigin,
			fldProxyAuthorization,
		};

		enum Method {
			mOPTIONS,
			mGET,
			mHEAD,
			mPOST,
			mPUT,
			mDELETE,
			mTRACE,
			mCONNECT
		};

		static ConstStrA getHeaderFieldName(Field fld);
		static ConstStrA getMethodName(Method m);

		static void cropWhite(ConstStrA &k);


	};

}


#endif /* JSONRPCSERVER_BREDY_HTTPSERVER_HEADERVALUE_H_ */
