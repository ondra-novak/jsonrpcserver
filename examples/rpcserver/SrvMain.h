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
#include "jsonrpc/jsonRpcServer.h"
#include "jsonrpc/server.h"


namespace httpexample {

using namespace LightSpeed;
using namespace BredyHttpSrv;
using namespace jsonsrv;


class SrvMain: public AbstractServerMain {
public:
	SrvMain();

	virtual natural onInitServer(const Args& args, SeqFileOutput serr, const IniConfig &config);
	virtual natural onStartServer(IHttpMapper &httpMapper);
	virtual void onLogRotate();

	AllocPointer<jsonrpc::Server> rpc;

	JSON::PNode rpcHelloWorld(RpcRequest *r);
	JSON::PNode rpcNumberSum(RpcRequest *r);
	JSON::PNode rpcReverse(RpcRequest *r);

};

} /* namespace qrpass */
#endif /* QRPASS_BREDY_SRVMAIN_H_ */
