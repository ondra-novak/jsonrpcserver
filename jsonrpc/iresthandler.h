/*
 * iresthandler.h
 *
 *  Created on: 31. 7. 2014
 *      Author: ondra
 */

#ifndef JSONSRV_BREDY_IRESTHANDLER_H_
#define JSONSRV_BREDY_IRESTHANDLER_H_
#include "lightspeed/base/actions/message.h"

#include "lightspeed/base/memory/sharedPtr.h"

#include "../httpserver/queryParser.h"
#include "../httpserver/httprequest.h"


#undef GET
#undef HEAD
#undef POST
#undef PUT
#undef DELETE
#undef OPTIONS
#undef PATCH
#undef TRACE

namespace jsonsrv {

using namespace LightSpeed;

///very simple REST api handler
/**
 * Handler allows to define endpoints handlers, similar to RPC handler
 */
class IRestHandler {
public:

	///request descriptor
	struct Request {
		///reference http request
		BredyHttpSrv::IHttpRequest &r;
		///parsed query
		BredyHttpSrv::QueryParser &q;

		enum Method {
			GET, HEAD, POST, PUT, DELETE, OPTIONS, PATCH, TRACE
		};

		Method method;

		Request(BredyHttpSrv::IHttpRequest &r,BredyHttpSrv::QueryParser &q, Method method)
			:r(r),q(q),method(method) {}

		void setResponseVLocation(ConstStrA vpath) {
			ConstStrA path = r.getPath();
			natural vpathpos = q.getPath().data() - path.data();
			StringA newpath = r.getBaseUrl() + path.head(vpathpos) + vpath;
			r.header(BredyHttpSrv::IHttpRequest::fldLocation, newpath);
		}
	};


	///Defined action
	typedef Message<natural,Request *> Action;


	///Adds new REST endpoint
	/**
	 * @param name name of endpoint. Name should not contain '/' You cannot add endpoints into deep tree. Just only first level can be used
	 * @param function called to handle this endpoint
	 */
	virtual void addEndpoint(StringA name, const Action &a) =0;
	///Removes endpoint
	virtual void removeEndpoint(ConstStrA name) =0;

};


typedef IRestHandler::Action RESTEndPt;

}


#endif /* JSONSRV_BREDY_IRESTHANDLER_H_ */
