/*
 * jsonRpcWebsockets.cpp
 *
 *  Created on: 9.6.2015
 *      Author: ondra
 */

#include "jsonRpcWebsockets.h"
#include "../httpserver/abstractWebSockets.tcc"
#include <lightspeed/base/interface.tcc>
#include "lightspeed/base/actions/promise.tcc"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/containers/map.tcc"

namespace jsonsrv {

class JsonRpcWebsocketsConnection::HttpRequestWrapper: public BredyHttpSrv::IHttpContextControl {
public:
	HttpRequestWrapper(BredyHttpSrv::IHttpContextControl *r, JsonRpcWebsocketsConnection *owner):r(r),owner(owner) {}

	BredyHttpSrv::IHttpContextControl  * const r;
	JsonRpcWebsocketsConnection * const owner;

protected:
	virtual ConstStrA getMethod() const {return "WS";}
	virtual ConstStrA getPath() const {return r->getPath();}
	virtual ConstStrA getProtocol() const {return r->getProtocol();}
	virtual HeaderValue getHeaderField(ConstStrA field) const {return r->getHeaderField(field);}
	virtual HeaderValue getHeaderField(HeaderField field) const {return r->getHeaderField(field);}
	virtual bool enumHeader(HdrEnumFn fn) const {return r->enumHeader(fn);}
	virtual ConstStrA getBaseUrl() const {return r->getBaseUrl();}
	virtual bool keepAlive() const {return true;}
	virtual void beginIO() {r->beginIO();}
	virtual void endIO()   {r->endIO();}

	virtual void *proxyInterface(IInterfaceRequest &p) {
		void *x = IInterface::proxyInterface(p);
		if (!x) x = r->proxyInterface(p);
		//protect underlying connection
		return x;
	}
	virtual const void *proxyInterface(const IInterfaceRequest &p) const {
		const void *x = IInterface::proxyInterface(p);
		if (!x) x = r->proxyInterface(p);
		//protect underlying connection
		return x;
	}

	virtual StringA getAbsoluteUrl() const { return r->getAbsoluteUrl(); }
	virtual StringA getAbsoluteUrl(ConstStrA relpath) const { return r->getAbsoluteUrl(relpath); }

	virtual void setRequestContext(IHttpHandlerContext *context) {
		r->setRequestContext(context);
	}
	virtual void setConnectionContext(IHttpHandlerContext *context) {
		owner->setContext(context);
	}
	virtual IHttpHandlerContext *getRequestContext() const {
		return r->getRequestContext();
	}
	virtual IHttpHandlerContext *getConnectionContext() const {
		return owner->getContext();
	}



};

class TmControlScope {
public:
	PNetworkStream stream;
	natural defTm;
	TmControlScope(PNetworkStream stream, IRpcNotify::TimeoutControl control)
		:stream(stream),defTm(stream->getTimeout()) {
		if (control == IRpcNotify::shortTimeout) stream->setTimeout(1);
	}
	~TmControlScope() {
		stream->setTimeout(defTm);
	}
};

JsonRpcWebsocketsConnection::JsonRpcWebsocketsConnection(IHttpRequest &request, IJsonRpc& handler, StringA openMethod)
	:WebSocketConnection(request),handler(handler),nextPromiseId(0),http(request),openMethod(openMethod.getMT()) {

	logobject = handler.getIfcPtr<IJsonRpcLogObject>();
	onCloseFuture.clear(StdAlloc::getInstance());
}

void JsonRpcWebsocketsConnection::sendNotification(ConstStrA name, JSON::ConstValue arguments, TimeoutControl tmControl) {
	Synchronized<FastLockR> _(lock);
	TmControlScope tmscope(this->stream,tmControl);
	JSON::ConstValue req = json("method",name)
			("params",arguments)
			("id",nil);
	ConstStrA msg = json.factory->toString(*req);
	this->sendTextMessage(msg,true);
}

Future<JsonRpcWebsocketsConnection::Result> JsonRpcWebsocketsConnection::callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context) {
	Synchronized<FastLockR> _(lock);
	natural promiseId = nextPromiseId++;
	JSON::Builder::CObject req = json.container("method",method)
			("params",params)
			("id",promiseId);

	if (context != null) {
		req("context",context);
	}

	ConstStrA msg = json.factory->toString(*req);
	this->sendTextMessage(msg,true);

	Future<Result> promise;
	waitingPromises.insert(promiseId,promise.getPromise());
	return promise;
}

JsonRpcWebsocketsConnection* JsonRpcWebsocketsConnection::getConnection(IHttpRequestInfo& request) {
	HttpRequestWrapper *rq = request.getIfcPtr<HttpRequestWrapper>();
	if (rq == 0) return 0;
	return rq->owner;
}

void JsonRpcWebsocketsConnection::setContext(IHttpHandlerContext* context) {
	this->context = context;
}

IHttpHandlerContext* JsonRpcWebsocketsConnection::getContext() const {
	return context;
}


void JsonRpcWebsocketsConnection::onTextMessage(ConstStrA msg) {
	Synchronized<FastLockR> _(lock);
	LS_LOGOBJ(lg);

	JSON::PNode req = json.factory->fromString(msg);
	if (req->getPtr("result")) {
		natural id = req["id"]->getUInt();
		const Promise<Result> *pres = waitingPromises.find(id);
		if (pres == 0) return;
		Promise<Result> p = *pres;
		waitingPromises.erase(id);
		if (req["error"]->isNull()) {
			Result res(req["result"],req["context"]);
			p.resolve(res);
		}
		else p.reject(RpcError(THISLOCATION,req["error"]));
	} else {
		TimeStamp beginTime = TimeStamp::now();
		ConstStrA method = req["method"]->getStringUtf8();
		JSON::PNode params = req["params"];
		JSON::PNode id = req["id"];
		JSON::PNode context = req->getPtr("context");
		IJsonRpc::CallResult callRes;
		HttpRequestWrapper httpwrp(&http,this);
		try
		{
			SyncReleased<FastLockR> _(lock);
			callRes = handler.callMethod(&httpwrp,method,params,context,id);
			if (logobject)
				logobject->logMethod(http,method,params,context,callRes.logOutput);
		} catch (std::exception &e) {
			callRes.result= json.factory->newValue(null);
			callRes.error = json.factory->newValue(e.what());
			callRes.id = id;
			callRes.logOutput = callRes.error;
			callRes.result = json.factory->newValue(null);
		}
		if (!id->isNull()) {
			JSON::Builder::Object response = json("result",callRes.result)
					("error",callRes.error)
					("id", callRes.id);
			if (callRes.newContext != nil) {
				response("context",callRes.newContext);
			}
			ConstStrA msg = json.factory->toString(*response);
			sendTextMessage(msg,true);
		}
		TimeStamp endTime = TimeStamp::now();
		natural ms = (endTime - beginTime).getMilis();
		lg.progress("%1 - POST %2 WSRPC/1.0 (%3 ms)") << http.getIfc<IHttpPeerInfo>().getPeerRealAddr()
						<< method << ms;
		http.recordRequestDuration(ms);
	}

}
JsonRpcWebsockets::JsonRpcWebsockets(IJsonRpc& handler,StringA openMethod):handler(handler),openMethod(openMethod.getMT()) {
}

WebSocketConnection* JsonRpcWebsockets::onNewConnection(
		IRuntimeAlloc& alloc, IHttpRequest& request,
		ConstStrA ) {

	IHttpRequest::HeaderValue origin =  request.getHeaderField(IHttpRequest::fldOrigin);
	bool allow = true;
	if (origin.defined) {
		Optional<bool> allowed = handler.isAllowedOrigin(origin);
		if (allowed == null) {
			ConstStrA corig = origin;

			if (corig.head(7) == ConstStrA("http://")) corig = corig.offset(7);
			else if (corig.head(8) == ConstStrA("https://")) corig = corig.offset(8);

			ConstStrA host = request.getHeaderField(IHttpRequest::fldHost);
			if (host.tail(corig.length()) != corig) allow = false;
		} else {
			allow = allowed;
		}
	}
	if (allow)
		return new(alloc) JsonRpcWebsocketsConnection(request,handler,openMethod);
	else
		throw HttpStatusException(THISLOCATION,request.getAbsoluteUrl(),403,"Forbidden");
}

void JsonRpcWebsocketsConnection::onCloseOutput(natural ) {
	onCloseFuture.getPromise().resolve();
	Synchronized<FastLockR> _(lock);
	context = nil;
}

JsonRpcWebsocketsConnection::~JsonRpcWebsocketsConnection() {
	onCloseFuture.getPromise().resolve();
	Synchronized<FastLockR> _(lock);
	context = nil;
}

void JsonRpcWebsocketsConnection::onConnect() {
	if (!openMethod.empty()) {
		IJsonRpc::CallResult callRes;
		{
			JSON::PNode nullNode = json.factory->newNullNode();
			JSON::PNode emptyArr = json.array();
			SyncReleased<FastLockR> _(lock);
			callRes = handler.callMethod(&http,openMethod,emptyArr,nullNode,nullNode);
			if (logobject)
				logobject->logMethod(http,openMethod,emptyArr,nullNode,callRes.logOutput);
			if (callRes.error != nil && !callRes.error->isNull()) {
				this->closeConnection(closeAbnormal);
			}
		}

	}
}

void JsonRpcWebsocketsConnection::dropConnection() {
	onCloseFuture.getPromise().resolve();
	Synchronized<FastLockR> _(lock);
	stream->closeOutput();
	context = nil;

}

void JsonRpcWebsocketsConnection::sendPrepared(const PreparedNotify& ntf, TimeoutControl tmControl) {
	Synchronized<FastLockR> _(lock);
	TmControlScope tmscope(this->stream,tmControl);
	this->sendTextMessage(ntf.content,true);
}

StringA buildPreparedNotify(const JSON::Builder& json, ConstStrA notifyName,
		JSON::ConstValue params) {
	JSON::ConstValue req = json("method", notifyName)("params", params)("id",
			json(nil));
	return json.factory->toString(*req);
}

PreparedNotify::PreparedNotify(ConstStrA notifyName, JSON::ConstValue params, const JSON::Builder &json)
	:content(buildPreparedNotify(json, notifyName, params)) {}

PreparedNotify JsonRpcWebsocketsConnection::prepare(
		LightSpeed::ConstStrA name, LightSpeed::JSON::ConstValue arguments) {
	Synchronized<FastLockR> _(lock);
	return PreparedNotify(name,arguments,json);
}

IRpcNotify *IRpcNotify::fromRequest(RpcRequest *r) {
	JsonRpcWebsocketsConnection *conn = JsonRpcWebsocketsConnection::getConnection(*r->httpRequest);
	return conn;
}

Future<void> JsonRpcWebsocketsConnection::onClose() {
	return onCloseFuture;
}

} /* namespace jsonsrv */

