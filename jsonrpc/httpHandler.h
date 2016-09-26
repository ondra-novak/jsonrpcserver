/*
 * httpHandler.h
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_H5046506504_HTTPHANDLER_H_
#define JSONRPC_JSONRPCSERVER_H5046506504_HTTPHANDLER_H_
#include "../httpserver/httprequest.h"
#include "idispatch.h"
#include "lightspeed/utils/FilePath.h"


namespace jsonrpc {

using namespace LightSpeed;

class HttpHandler: public BredyHttpSrv::IHttpHandler {
public:
	HttpHandler(IDispatcher &dispatcher);
	HttpHandler(IDispatcher &dispatcher, const JSON::Builder &json);


	void setClientPage(const FilePath &path);
	void unsetClientPage();

protected:
	virtual natural onRequest(BredyHttpSrv::IHttpRequest& request, ConstStrA vpath);
	virtual natural onData(BredyHttpSrv::IHttpRequest& request);

	virtual natural onGET(BredyHttpSrv::IHttpRequest&,  ConstStrA );
	virtual natural onPOST(BredyHttpSrv::IHttpRequest& , ConstStrA );
	virtual natural onOtherMethod(BredyHttpSrv::IHttpRequest& , ConstStrA ) {return 403;}

//	natural dumpMethods(ConstStrA name, BredyHttpSrv::IHttpRequest& request);
	natural sendClientJs(BredyHttpSrv::IHttpRequest& request);
	natural sendWsClientJs(BredyHttpSrv::IHttpRequest& request);

	class IRequestContext: public BredyHttpSrv::IHttpHandlerContext {
	public:
		virtual natural onData(BredyHttpSrv::IHttpRequest& request) = 0;
	};


	IDispatcher &dispatcher;
	JSON::Builder json;

	class RpcContext;
	AllocPointer<BredyHttpSrv::IHttpHandler> webClient;
	StringA clientFile;
	StringA methodListTag;

	natural dumpMethods(ConstStrA name, BredyHttpSrv::IHttpRequest& request);




};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONRPCSERVER_H5046506504_TTPHANDLER_H_ */
