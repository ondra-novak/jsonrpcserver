/*
 * httpHandler.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "httpHandler.h"

#include <lightspeed/base/actions/executor.h>
#include "lightspeed/base/streams/fileio.h"

#include "lightspeed/base/actions/promise.h"

#include "../httpserver/queryParser.h"
#include "lightspeed/base/text/textParser.tcc"
#include "../httpserver/simpleWebSite.h"
#include "errors.h"

using LightSpeed::parseUnsignedNumber;
namespace jsonrpc {

using namespace BredyHttpSrv;

class HttpHandler::RpcContext: public HttpHandler::IRequestContext {
public:
	RpcContext(natural version, IHttpRequest &request, HttpHandler &owner)
		:version(version),request(request),owner(owner) {}

	virtual natural onData(IHttpRequest &request);

	void onResult(const Response &v, const JSON::ConstValue &id);
	void onError(const PException& e, const JSON::ConstValue &id);

	class Action;

protected:
	natural version;
	IHttpRequest &request;
	HttpHandler &owner;
	//result is stored here before it is send to the caller
	//it can be created in another thread
	//once connection is wakenUo, the value is grabbed and serialized to the output
	JSON::ConstValue sendOnWakeUp;


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
	natural version = 1;

	QueryParser qp(vpath);
	while (qp.hasItems()) {
		const QueryField &qf = qp.getNext();
		if (qf.name == ConstStrA("ver")) {
			parseUnsignedNumber(qf.value.getFwIter(),version,10);
		}
	}

	RpcContext *ctx = new RpcContext(version,request,*this);
	request.setRequestContext(ctx);

	return stContinue;


}

natural HttpHandler::RpcContext::onData(IHttpRequest& request) {

	if (sendOnWakeUp != null) {
		SeqFileOutput output(&request);
		owner.json.factory->toStream(*sendOnWakeUp,output);
		sendOnWakeUp = null;
		return stContinue;
	}

	if (!request.canRead()) return 0;

	JSON::ConstValue val; try {
		SeqFileInput input(&request);
		val = owner.json.factory->fromStream(input);
	} catch (std::exception &e) {
		request.errorPage(400,ConstStrA(),e.what());
	}

	JSON::ConstValue id;
	Future<Response> futureresult;
/*	id = owner.dispatcher.dispatchMessage(val,version,owner.json,&request,futureresult.getPromise());
	//this will carried instantly when function resolves the promise during its run
	futureresult.thenCall(Message<void, JSON::ConstValue>::create(this,&HttpHandler::RpcContext::onResult,id),
					Message<void, PException>::create(this,&HttpHandler::RpcContext::onError,id));
	//moves execution back to the top of the function */
	return stSleep;

}

void HttpHandler::setClientPage(const FilePath& path) {
	webClient = new BredyHttpSrv::SimpleWebSite(path,0);
}
void HttpHandler::unsetClientPage() {
	webClient = null;
}


natural HttpHandler::onGET(BredyHttpSrv::IHttpRequest&r, ConstStrA vpath) {
	if (vpath.empty()) {
		r.redirect("+/",303);return 0;
	}
	if (vpath[0] != '/') return stNotFound;
	if (webClient == null) {
		r.header(r.fldAllow,"POST, OPTIONS");
		return stMethodNotAllowed;
	} else {
		return r.forwardRequestTo(webClient,vpath.offset(1));
	}


}

void HttpHandler::RpcContext::onResult(const Response &v, const JSON::ConstValue &id) {

	if (id != null && !id->isNull()) {
		JSON::Builder::CObject response = owner.json("id",id)
			("error",null)
			("result",v.result);
		if (v.context) response("context",v.context);
		sendOnWakeUp = response;
		request.wakeUp(0);
	}
}

void HttpHandler::RpcContext::onError(const PException& e, const JSON::ConstValue &id) {

	const RpcException *rpce = dynamic_cast<const RpcException *>(e.get());
	if (rpce == 0) {
		return onError(UncauchException(THISLOCATION,*e).clone(),id);
	} else {
		JSON::ConstValue response = owner.json("id",id)
			("error",rpce->getJSON(owner.json))
			("result",null);
		sendOnWakeUp = response;
		request.wakeUp(0);
	}
}

} /* namespace jsonrpc */

