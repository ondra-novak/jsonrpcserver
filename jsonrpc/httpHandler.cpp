/*
 * httpHandler.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "httpHandler.h"

#include <lightspeed/base/actions/executor.h>
#include "lightspeed/base/streams/fileio.h"

#include "lightspeed/base/actions/promise.tcc"

#include "../httpserver/queryParser.h"
#include "lightspeed/base/text/textParser.tcc"
#include "../httpserver/simpleWebSite.h"
#include "errors.h"
#include "rpc.js.h"
#include "lightspeed/base/text/toString.tcc"

#include "lightspeed/utils/md5iter.h"

#include "lightspeed/base/interface.tcc"
#include "methodreg.h"
using LightSpeed::parseUnsignedNumber;
namespace jsonrpc {

using namespace BredyHttpSrv;

class HttpHandler::RpcContext: public HttpHandler::IRequestContext {
public:
	RpcContext(IHttpRequest &request, HttpHandler &owner)
		:request(request),owner(owner) {}

	virtual natural onData(IHttpRequest &request);

protected:
	IHttpRequest &request;
	HttpHandler &owner;
	Future<JSON::ConstValue> result;


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
	RpcContext *ctx = new RpcContext(request,*this);
	request.setRequestContext(ctx);

	return stContinue;


}

natural HttpHandler::RpcContext::onData(IHttpRequest& request) {

	const JSON::ConstValue *res = result.tryGetValue();
	if (res) {
		SeqFileOutput output(&request);
		owner.json.factory->toStream(*(*res),output);
		result.clear();
		return stContinue;
	}

	if (!request.canRead()) return 0;

	JSON::ConstValue val;
	try {
		SeqFileInput input(&request);
		val = owner.json.factory->fromStream(input);
	} catch (std::exception &e) {
		request.errorPage(400,ConstStrA(),e.what());
	}

	owner.dispatcher.dispatchMessage(val,owner.json,&request,result.getPromise());
	result.thenWake(request);
	return stSleep;

}

void HttpHandler::setClientPage(const FilePath& path) {
	webClient = new BredyHttpSrv::SimpleWebSite(FilePath(path.getPath()),0);
	clientFile = String::getUtf8(path.getFilename());
}
void HttpHandler::unsetClientPage() {
	webClient = null;
}


natural HttpHandler::onGET(BredyHttpSrv::IHttpRequest&r, ConstStrA vpath) {

	if (vpath.head(9)==ConstStrA("/methods/")) {
		return dumpMethods(vpath.offset(9),r);
	} else
	if (vpath == ConstStrA("/client.js")) {
		return sendClientJs(r);
	} else if (vpath == ConstStrA("/ws_client.js")) {
		return sendWsClientJs(r);
	}

	if (webClient == null) {
		r.header(r.fldAllow,"POST, OPTIONS");
		return stMethodNotAllowed;
	} else {
		if (vpath.empty()) vpath = clientFile;
		return r.forwardRequestTo(webClient,vpath);
	}
}


natural HttpHandler::dumpMethods(ConstStrA name, IHttpRequest& request) {

	if (name.tail(3) != ConstStrA(".js")) return 404;
	ConstStrA varname = name.crop(0,3);

	HeaderValue income_etag = request.getHeaderField(IHttpRequest::fldIfNoneMatch);
	if (income_etag.defined) {
		StringA chkTag = methodListTag;
		if (income_etag == chkTag) return stNotModified;
	}


	JSON::PFactory fact = JSON::create();
	JSON::Value arr = fact->array();
	ConstStrA prevMethod;

	IMethodRegister &mreg = dispatcher.getIfc<IMethodRegister>();

	class MethodReceiver: public IMethodRegister::IMethodEnum {
	public:
		MethodReceiver(JSON::Value &arr, JSON::PFactory &fact):arr(arr),fact(fact) {}
		virtual void operator()(ConstStrA prototype) const {
			ConstStrA baseName = prototype.split(':').getNext();
			if (baseName == prevMethod) return;
			arr->add(fact->newValue(baseName));
		}

		JSON::Value &arr;
		JSON::PFactory &fact;
		ConstStrA prevMethod;
	};

	mreg.enumMethods(MethodReceiver(arr,fact));

	ConstStrA jsonstr = fact->toString(*arr);
	HashMD5<char> hash;
	hash.blockWrite(jsonstr,true);
	hash.finish();
	StringA digest = hash.hexdigest();
	methodListTag = digest.getMT();
	StringA etag = methodListTag;
	StringA result = ConstStrA("var ") + varname + ConstStrA("=") + jsonstr + ConstStrA(";\r\n");

	request.header(IHttpRequest::fldContentType,"application/javascript");
	request.header(IHttpRequest::fldContentLength,ToString<natural>(result.length()));
	request.header(IHttpRequest::fldETag,etag);
	request.writeAll(result.data(),result.length());

	return stOK;
}


natural HttpHandler::sendClientJs(IHttpRequest& request) {
	ConstBin data(reinterpret_cast<const byte *>(jsonrpcserver_rpc_js),jsonrpcserver_rpc_js_length);
	request.header(IHttpRequest::fldContentType,"application/javascript");
	request.header(IHttpRequest::fldContentLength,ToString<natural>(data.length()));
	request.header(IHttpRequest::fldCacheControl,"max-age=31556926");
	request.writeAll(data.data(),data.length());
	return stOK;
}

natural HttpHandler::sendWsClientJs(IHttpRequest& request) {
	ConstBin data(reinterpret_cast<const byte *>(jsonrpcserver_wsrpc_js),jsonrpcserver_wsrpc_js_length);
	request.header(IHttpRequest::fldContentType,"application/javascript");
	request.header(IHttpRequest::fldContentLength,ToString<natural>(data.length()));
	request.header(IHttpRequest::fldCacheControl,"max-age=31556926");
	request.writeAll(data.data(),data.length());
	return stOK;
}


} /* namespace jsonrpc */

