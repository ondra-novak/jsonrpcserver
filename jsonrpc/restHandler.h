/*
 * RestHandler.h
 *
 *  Created on: 31. 7. 2014
 *      Author: ondra
 */

#ifndef JSONSRV_BREDY_RESTHANDLER_H_
#define JSONSRV_BREDY_RESTHANDLER_H_

#include <lightspeed/base/containers/stringKey.h>
#include <lightspeed/base/actions/message.h>
#include <lightspeed/base/memory/sharedPtr.h>

#include "../httpserver/queryParser.h"
#include "../httpserver/httprequest.h"

#include "iresthandler.h"
#include "lightspeed/base/containers/map.h"




namespace jsonsrv {

using namespace LightSpeed;

///REST handler implementation
class RestHandler: public BredyHttpSrv::IHttpHandler, public IRestHandler {
public:

	virtual void addEndpoint(StringA name, const Action &a);
	virtual void removeEndpoint(ConstStrA name);

	virtual natural onRequest(BredyHttpSrv::IHttpRequest &request, ConstStrA vpath);
	virtual natural onData(BredyHttpSrv::IHttpRequest &request) {return 0;}

protected:

	typedef Map<StringKey<StringA>, Action> EndpointMap;
	EndpointMap endpoints;

};

} /* namespace coinstock */

#endif /* JSONSRV_BREDY_RESTHANDLER_H_ */
