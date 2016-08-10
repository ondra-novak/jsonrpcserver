/*
 * httpHandler.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "httpHandler.h"

#include "lightspeed/base/streams/fileio.h"

#include "lightspeed/base/actions/promise.h"
namespace jsonrpc {

using namespace BredyHttpSrv;

class HttpHandler::RpcContext: public HttpHandler::IRequestContext {
public:
	RpcContext(natural version, IHttpRequest &request, HttpHandler &owner)
		:version(version),request(request),owner(owner) {}

	virtual natural onData(IHttpRequest &request);

protected:
	natural version;
	IHttpRequest &request;
	HttpHandler &owner;


};


HttpHandler::HttpHandler(IDispatcher& dispatcher):dispatcher(dispatcher) {
}

HttpHandler::HttpHandler(IDispatcher& dispatcher,const JSON::Builder &json):dispatcher(dispatcher),json(json) {
}

natural HttpHandler::onRequest(IHttpRequest& request, ConstStrA vpath) {
	if (request.getMethod() == "GET") return onGET(request, vpath);
	else if (request.getMethod() == "POST") return onPOST(request, vpath);
	else return onOtherMethod(request,vpath);
}

natural HttpHandler::onData(IHttpRequest& request) {
	IRequestContext *ctx = static_cast<IRequestContext *>(request.getRequestContext());
	if (ctx) {
		return ctx->onData(request);
	} else {
		return 500;
	}
}

natural HttpHandler::onPOST(IHttpRequest& request, ConstStrA vpath) {
	request.header(IHttpRequest::fldContentType,"application/json");







	return stContinue;


}

natural HttpHandler::RpcContext::onData(IHttpRequest& request) {

	if (!request.canRead()) return 0;

	JSON::ConstValue val; {
		SeqFileInput input(&request);
		val = owner.json.factory->fromStream(input);
	}

	Future<JSON::ConstValue> futureresult;
	owner.dispatcher.dispatchMessage(val,version,owner.json,&request,futureresult.getPromise());
	const JSON::ConstValue *v = futureresult.tryGetValue();
	if (v != 0) {
				SeqFileOutput output(&request);
				owner.json.factory->toStream(*(*v),output);
				return stContinue;
	} else {
		/*futureresult.then()*/
		return stDetach;
	}

}


} /* namespace jsonrpc */

