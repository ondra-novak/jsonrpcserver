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
	virtual natural onData(IHttpRequest &) {return false;}


protected:
	///Serves the file
	/** Default implemenation just reads the file from the filesystem and outputs it to the http stream
	 * Overwriting this method can extend function to perform various task with the file, for
	 * example run template engine or interpret script
	 *
	 * @param request request object
	 * @param pathName pathname of the file to serve
	 * @param contentType content type determined from the file's extension
	 * @param vpath vpath
	 * @return status code
	 */
	natural serverFile(IHttpRequest& request, ConstStrW pathName, ConstStrA contentType, ConstStrA vpath);

	FilePath documentRoot;
	StringA cacheStr;
};

} /* namespace BredyHttpSrv */
#endif /* BREDYHTTP_SIMPLEWEBSITEHANDLER_H_ */
