/*
 * simpleWebSiteHandler.h
 *
 *  Created on: Sep 19, 2012
 *      Author: ondra
 */

#ifndef BREDYHTTP_SIMPLEWEBSITEHANDLER_H_
#define BREDYHTTP_SIMPLEWEBSITEHANDLER_H_

#include "httprequest.h"
#include "lightspeed/utils/FilePath.h"


namespace BredyHttpSrv {

using namespace LightSpeed;

class SimpleWebSite: public IHttpHandler {
public:
	SimpleWebSite(FilePath documentRoot, natural cacheTime = 86400);
	virtual natural onRequest(IHttpRequest &request, ConstStrA vpath);
	virtual natural onData(IHttpRequest &request) {return false;}


protected:
	FilePath documentRoot;
	StringA cacheStr;
};

} /* namespace BredyHttpSrv */
#endif /* BREDYHTTP_SIMPLEWEBSITEHANDLER_H_ */
