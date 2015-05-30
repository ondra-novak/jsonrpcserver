/*
 * SrvMain.h
 *
 *  Created on: 31.3.2015
 *      Author: ondra
 */

#ifndef QRPASS_BREDY_SRVMAIN_H_
#define QRPASS_BREDY_SRVMAIN_H_

//#include "httpserver\httprequest.h"
#include "httpserver/serverMain.h"
#include "httpserver/simpleWebSite.h"


namespace httpexample {

using namespace LightSpeed;
using namespace BredyHttpSrv;


class SrvMain: public AbstractServerMain {
public:
	SrvMain();

	virtual natural onInitServer(const Args& args, SeqFileOutput serr, const IniConfig &config);
	virtual natural onStartServer(IHttpMapper &httpMapper);

	AllocPointer<SimpleWebSite> website;

};

} /* namespace qrpass */
#endif /* QRPASS_BREDY_SRVMAIN_H_ */
