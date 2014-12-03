/*
 * RestHandler.cpp
 *
 *  Created on: 31. 7. 2014
 *      Author: ondra
 */

#include "restHandler.h"

#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/base/exceptions/httpStatusException.h"

namespace jsonsrv {


void RestHandler::addEndpoint(StringA name, const Action& a) {
	endpoints.replace(StringKey<StringA>(name),a);
	LS_LOG.progress("Added REST endpoint: %1") << name;
}

void RestHandler::removeEndpoint(ConstStrA name) {
	endpoints.erase(StringKey<StringA>(name));
	LS_LOG.progress("Removed REST endpoint: %1") << name;
}

natural RestHandler::onRequest(BredyHttpSrv::IHttpRequest& request,ConstStrA vpath) {
	BredyHttpSrv::QueryParser p(vpath);
	ConstStrA path = p.getPath();
	ConstStrA::SplitIterator piter = path.split('/');
	ConstStrA endpointName = piter.getNext();
	if (endpointName.empty() && piter.hasItems()) endpointName = piter.getNext();

	BredyHttpSrv::QueryParser p2(vpath.offset(endpointName.length() + (endpointName.data() - vpath.data()) ));

	const Action *a = endpoints.find(StringKey<StringA>(endpointName));
	if (a == 0) return stNotFound;

	Request::Method m;
	StrCmpCI<char> cmp;
	ConstStrA strMethod = request.getMethod();
	if (cmp(strMethod,"GET") == cmpResultEqual) m = Request::GET;
	else if (cmp(strMethod,"HEAD") == cmpResultEqual) m = Request::HEAD;
	else if (cmp(strMethod,"POST") == cmpResultEqual) m = Request::POST;
	else if (cmp(strMethod,"PUT") == cmpResultEqual) m = Request::PUT;
	else if (cmp(strMethod,"DELETE") == cmpResultEqual) m = Request::DELETE;
	else if (cmp(strMethod,"OPTIONS") == cmpResultEqual) m = Request::OPTIONS;
	else if (cmp(strMethod,"PATCH") == cmpResultEqual) m = Request::PATCH;
	else if (cmp(strMethod,"TRACE") == cmpResultEqual) m = Request::TRACE;
	else {
		return stMethodNotAllowed;
	}


	ConstStrA restPath = vpath.offset(endpointName.length()+1);
	if (!restPath.empty() && restPath[0] =='/') restPath = restPath.offset(1);
	Request r(request,p2, restPath,m);
	return a->deliver(&r);
}


} /* namespace coinstock */
