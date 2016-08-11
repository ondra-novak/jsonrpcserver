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

using LightSpeed::parseUnsignedNumber;
namespace jsonrpc {

using namespace BredyHttpSrv;

class HttpHandler::RpcContext: public HttpHandler::IRequestContext {
public:
	RpcContext(natural version, IHttpRequest &request, HttpHandler &owner)
		:version(version),request(request),owner(owner) {}

	virtual natural onData(IHttpRequest &request);

	void onResultDetached(JSON::ConstValue v);
	void onResultDetached2(JSON::ConstValue v);
	void onResultAttached(JSON::ConstValue v);
	natural onError(const Exception &e);

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

	if (!request.canRead()) return 0;

	JSON::ConstValue val; {
		SeqFileInput input(&request);
		val = owner.json.factory->fromStream(input);
	}

	Future<JSON::ConstValue> futureresult;
	owner.dispatcher.dispatchMessage(val,version,owner.json,&request,futureresult.getPromise());
	try {
		const JSON::ConstValue *v = futureresult.tryGetValue();
		if (v != 0) {
			onResultAttached(*v);
			return stContinue;
		} else {
			futureresult.thenCall(Message<void, JSON::ConstValue>::create(this,&HttpHandler::RpcContext::onResultDetached));
			return stDetach;
		}
	} catch (Exception &e) {
		return onError(e);
	} catch (std::exception &e) {
		return onError(StdException(THISLOCATION,e));
	} catch (...) {
		return onError(UnknownException(THISLOCATION));
	}

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

void HttpHandler::RpcContext::onResultDetached(JSON::ConstValue v) {
	IExecutor &executor = request.getIfc<IServerSvcs>().getExecutor();
	executor.execute(IExecutor::ExecAction::create(this,&HttpHandler::RpcContext::onResultDetached,v));
}


void HttpHandler::RpcContext::onResultDetached2(JSON::ConstValue v) {
	onResultAttached(v);
	request.attachThread(stContinue);
}

void HttpHandler::RpcContext::onResultAttached(JSON::ConstValue v) {
	try {
		SeqFileOutput output(&request);
		owner.json.factory->toStream(*v,output);
	} catch (...) {
		//failed to deliver response, ignore exeception
	}

}


} /* namespace jsonrpc */

