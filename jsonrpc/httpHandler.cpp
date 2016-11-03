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

#include "ipeer.h"
#include "methodreg.h"
#include "json.h"
using LightSpeed::parseUnsignedNumber;
namespace jsonrpc {

using namespace BredyHttpSrv;

class HttpHandler::RpcContext: public HttpHandler::IRequestContext, public IPeer {
public:
	RpcContext(IHttpRequest &request, HttpHandler &owner,natural version)
		:request(request),owner(owner),result(null),me(this),version(version) {}
	~RpcContext() {
		//if request is being destroyed, we need to cancel the future now.
		result.cancel();
		me.setNull();
	}

	virtual natural onData(IHttpRequest &request);
	natural sendResponse();

protected:
	IHttpRequest &request;
	HttpHandler &owner;
	Future<JValue> result;
	ContextVar requestContext;
	WeakRefTarget<IPeer> me;
	natural version;

	void wakeUpConnection(const JValue &);

	virtual BredyHttpSrv::IHttpRequestInfo *getHttpRequest() const {
		return &request;
	}
	virtual ConstStrA getName() const {
		return request.getIfc<BredyHttpSrv::IHttpPeerInfo>().getPeerRealAddr();
	}
	virtual natural getPortIndex() const {
		return request.getIfc<BredyHttpSrv::IHttpPeerInfo>().getSourceId();
	}
	virtual IRpcNotify *getNotifySvc() const {
		return 0;
	}
	virtual void setContext(Context *ctx) {
		requestContext = ctx;
	}
	virtual Context *getContext() const {
		return requestContext;
	}
	virtual IClient *getClient() const {
		return 0;
	}
	virtual natural getVersion() const {
		return version;
	}


};


HttpHandler::HttpHandler(IDispatcher& dispatcher):dispatcher(dispatcher) {
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
	natural version = getVersionFromReq(request,vpath);
	request.header(IHttpRequest::fldContentType,"application/json");
	RpcContext *ctx = new RpcContext(request,*this,version);
	request.setRequestContext(ctx);

	return stContinue;


}

natural HttpHandler::RpcContext::sendResponse() {
const JValue *res = result.tryGetValue();
if (res) {
	SeqFileOutput output(&request);
	res->serialize([&](char c){output.write(c);});
	result.clear();
	return stContinue;
} else {
	result.thenCall(Message<void,JValue,void>::create(this, &HttpHandler::RpcContext::wakeUpConnection));
	return stSleep;
}
}

natural HttpHandler::RpcContext::onData(IHttpRequest& request) {

	IHttpRequest::EventType evType = request.getEventType();
	switch (evType) {
	case IHttpRequest::evUserWakeup: return sendResponse();
	case IHttpRequest::evEndOfStream: return stOK;
	case IHttpRequest::evDataReady: {
			JValue val;
			try {
				SeqFileInput input(&request);
				val = JValue::parse([&](){return input.getNext();});
			} catch (std::exception &e) {
				request.errorPage(400,ConstStrA(),e.what());
			}

			JValue v = val["method"];
			if (v != null) request.setRequestName(convStr(v.getString()));
			request.sendHeaders();

			result = owner.dispatcher.dispatchMessage(val,me);
			return sendResponse();
		}
	default: return stInternalError;
	}
}

void HttpHandler::setClientPage(const FilePath& path) {
	webClient = new BredyHttpSrv::SimpleWebSite(FilePath(path.getPath()),0);
	clientFile = String::getUtf8(path.getFilename());
}
void HttpHandler::unsetClientPage() {
	webClient = null;
}


natural HttpHandler::onGET(BredyHttpSrv::IHttpRequest&r, ConstStrA vpath) {

	QueryParser qp(vpath);
	ConstStrA path = qp.getPath();

	if (path.head(9)==ConstStrA("/methods/")) {
		natural version = getVersionFromReq(r,vpath);
		return dumpMethods(vpath.offset(9),version,r);
	} else
		if (path == ConstStrA("/client.js")) {
			return sendClientJs(r);
	} else if (path == ConstStrA("/ws_client.js")) {
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


natural HttpHandler::dumpMethods(ConstStrA name, natural version, IHttpRequest& request) {

	if (name.tail(3) != ConstStrA(".js")) return 404;
	ConstStrA varname = name.crop(0,3);

	HeaderValue income_etag = request.getHeaderField(IHttpRequest::fldIfNoneMatch);
	if (income_etag.defined) {
		StringA chkTag = methodListTag;
		if (income_etag == chkTag) return stNotModified;
	}


	JArray arr;
	ConstStrA prevMethod;

	IMethodRegister &mreg = dispatcher.getIfc<IMethodRegister>();

	class MethodReceiver: public IMethodRegister::IMethodEnum {
	public:
		MethodReceiver(JArray &arr):arr(arr) {}
		virtual void operator()(StrView prototype) const {
			StrView baseName = prototype.split(':').getNext();
			if (baseName == StrView(prevMethod)) return;
			arr.add(JValue(baseName));
		}

		JArray &arr;
		ConstStrA prevMethod;
	};

	mreg.enumMethods(MethodReceiver(arr), version);

	JString jsonstr = JValue(arr).stringify();
	HashMD5<char> hash;
	hash.blockWrite(convStr(jsonstr),true);
	hash.finish();
	StringA digest = hash.hexdigest();
	methodListTag = digest.getMT();
	StringA etag = methodListTag;
	StringA result = ConstStrA("var ") + varname + ConstStrA("=") + convStr(jsonstr) + ConstStrA(";\r\n");

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


void HttpHandler::RpcContext::wakeUpConnection(const JValue &) {
	request.wakeUp(0);
}

const char *HttpHandler::versionHdr = "Version";

natural HttpHandler::getVersionFromReq(BredyHttpSrv::IHttpRequestInfo& req, ConstStrA vpath) {
	QueryParser qp(vpath);
	bool verset = false;
	natural version = 0;
	while (qp.hasItems()) {
		const QueryField &fld = qp.getNext();
		if (fld.name == "v" || fld.name == "ver" || fld.name == "version") {
			verset = true;
			if (fld.value == "max") version = naturalNull;
			else parseUnsignedNumber(fld.value.getFwIter(),version,10);
		}
	}
	if (!verset) {
		HeaderValue hv = req.getHeaderField(versionHdr);
		if (hv.defined) {
			if (hv == "max") version = naturalNull;
			else parseUnsignedNumber(hv.getFwIter(),version,10);
		}
	}
	return version;
}

} /* namespace jsonrpc */

