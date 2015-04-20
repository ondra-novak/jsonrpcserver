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


	EndpointMap::Iterator found = endpoints.seek(StringKey<StringA>(path),NULL);
	if (!found.hasItems()) return stNotFound;
	const EndpointMap::Entity &e = found.getNext();
	if (path.head(e.key.length()) != e.key) return stNotFound;
	const Action *a = &e.value;

	BredyHttpSrv::QueryParser p2(vpath.offset(e.key.length()));


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


	Request r(request,p2, m);
	return a->deliver(&r);
}

bool RestHandler::Cmp::operator ()(ConstStrA a, ConstStrA b) const {
	return a > b;
}


} /* namespace coinstock */

