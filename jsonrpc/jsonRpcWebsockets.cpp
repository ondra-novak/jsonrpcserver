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
namespace jsonsrv {

class JsonRpcWebsocketsConnection::HttpRequestWrapper: public BredyHttpSrv::IHttpRequest {
public:
	HttpRequestWrapper(BredyHttpSrv::IHttpRequest *r, JsonRpcWebsocketsConnection *owner):r(r),owner(owner) {}

	BredyHttpSrv::IHttpRequest  * const r;
	JsonRpcWebsocketsConnection * const owner;

protected:
	virtual ConstStrA getMethod() const {return "WS";}
	virtual ConstStrA getPath() const {return r->getPath();}
	virtual ConstStrA getProtocol() const {return r->getProtocol();}
	virtual HeaderValue getHeaderField(ConstStrA field) const {return r->getHeaderField(field);}
	virtual HeaderValue getHeaderField(HeaderField field) const {return r->getHeaderField(field);}
	virtual bool enumHeader(HdrEnumFn fn) const {return r->enumHeader(fn);}
	virtual void sendHeaders() {}
	virtual void header(ConstStrA , ConstStrA ) {}
	virtual void header(HeaderField , ConstStrA ) {}
	virtual void status(natural , ConstStrA = ConstStrA()) {}
	virtual void errorPage(natural , ConstStrA = ConstStrA(), ConstStrA = ConstStrA()) {}
	virtual void redirect(ConstStrA , int = 0) {}
	virtual ConstStrA getBaseUrl() const {return r->getBaseUrl();}
	virtual void useHTTP11(bool ) {}
	virtual bool isField(ConstStrA text, HeaderField fld) const {return r->isField(text,fld);}
	virtual natural read(void *,  natural ) {return 0;}
    virtual natural write(const void *,  natural )  {return 0;}
	virtual natural peek(void *, natural ) const {return 0;}
	virtual bool canRead() const {return false;}
	virtual bool canWrite() const {return false;}
	virtual void flush() {}
	virtual natural dataReady() const {return 0;}
	virtual void finish() {}
	virtual bool headersSent() const {return true;}
	virtual natural callHandler(ConstStrA , IHttpHandler ** = 0) {return 0;}
	virtual natural forwardRequest(ConstStrA, IHttpHandler ** = 0) { return 0; }
	virtual bool keepAlive() const {return true;}
	virtual PNetworkStream getConnection() {return nil;}
	virtual void setRequestContext(IHttpHandlerContext *context) {
		rctx = context;
	}
	virtual void setConnectionContext(IHttpHandlerContext *context) {
		owner->jsonrpc_context = context;
	}
	virtual IHttpHandlerContext *getRequestContext() const {
		return rctx;
	}
	virtual IHttpHandlerContext *getConnectionContext() const {
		return owner->jsonrpc_context;
	}
	virtual natural getPostBodySize() const {
		return 0;
	}
	virtual void beginIO() {r->beginIO();}
	virtual void endIO()   {r->endIO();}
	virtual void setMaxPostSize(natural ) {}
	virtual void attachThread(natural ) {}
	virtual void closeOutput() {}
	virtual void setRequestName(ConstStrA ) {}

	virtual void *proxyInterface(IInterfaceRequest &p) {
		void *x = IInterface::proxyInterface(p);
		if (!x) x = r->proxyInterface(p);
		return x;
	}
	virtual const void *proxyInterface(const IInterfaceRequest &p) const {
		const void *x = IInterface::proxyInterface(p);
		if (!x) x = r->proxyInterface(p);
		return x;
	}

	virtual StringA getAbsoluteUrl() const { return r->getAbsoluteUrl(); }
	virtual StringA getAbsoluteUrl(ConstStrA relpath) const { return r->getAbsoluteUrl(relpath); }



	AllocPointer<IHttpHandlerContext> rctx;

};


JsonRpcWebsocketsConnection::JsonRpcWebsocketsConnection(IHttpRequest &request, IJsonRpc& handler, StringA openMethod)
	:WebSocketConnection(request),handler(handler),nextPromiseId(0),http(request),openMethod(openMethod.getMT()) {

	logobject = handler.getIfcPtr<IJsonRpcLogObject>();
}

void JsonRpcWebsocketsConnection::sendNotification(ConstStrA name, JSON::PNode arguments) {
	Synchronized<FastLock> _(lock);
	JSON::PNode req = json("method",name)
			("params",arguments)
			("id",nil);
	ConstStrA msg = json.factory->toString(*req);
	this->sendTextMessage(msg,true);
}

Future<JSON::PNode> JsonRpcWebsocketsConnection::callMethod(ConstStrA name, JSON::PNode arguments) {
	Synchronized<FastLock> _(lock);
	natural promiseId = nextPromiseId++;
	JSON::PNode req = json("method",name)
			("params",arguments)
			("id",promiseId);
	ConstStrA msg = json.factory->toString(*req);
	this->sendTextMessage(msg,true);

	Future<JSON::PNode> promise;
	waitingPromises.insert(promiseId,promise.getPromise());
	return promise;
}

JsonRpcWebsocketsConnection* JsonRpcWebsocketsConnection::getConnection(IHttpRequest& request) {
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
	Synchronized<FastLock> _(lock);
	LS_LOGOBJ(lg);

	JSON::PNode req = json.factory->fromString(msg);
	if (req->getPtr("result")) {
		natural id = req["id"]->getUInt();
		const Promise<JSON::PNode> *pres = waitingPromises.find(id);
		if (pres == 0) return;
		Promise<JSON::PNode> p = *pres;
		waitingPromises.erase(id);
		if (req["error"]->isNull()) p.resolve(req["result"]);
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
			SyncReleased<FastLock> _(lock);
			callRes = handler.callMethod(&httpwrp,method,params,context,id);
			if (logobject)
				logobject->logMethod(http,method,params,context,callRes.logOutput);
		} catch (std::exception &e) {
			callRes.error = json.factory->newValue(e.what());
			callRes.id = id;
			callRes.logOutput = callRes.error;
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
		lg.progress("%1 - POST %2 WSRPC/1.0 (%3 ms)") << http.getIfc<IHttpPeerInfo>().getPeerRealAddr()
						<< method << (endTime - beginTime).getMilis();
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
	Synchronized<FastLock> _(lock);
	context = nil;
}

JsonRpcWebsocketsConnection::~JsonRpcWebsocketsConnection() {
	Synchronized<FastLock> _(lock);
	context = nil;
}

void JsonRpcWebsocketsConnection::onConnect() {
	if (!openMethod.empty()) {
		IJsonRpc::CallResult callRes;
		{
			JSON::PNode nullNode = json.factory->newNullNode();
			JSON::PNode emptyArr = json.array();
			SyncReleased<FastLock> _(lock);
			callRes = handler.callMethod(&http,openMethod,emptyArr,nullNode,nullNode);
			if (logobject)
				logobject->logMethod(http,openMethod,emptyArr,nullNode,callRes.logOutput);
			if (callRes.error != nil && !callRes.error->isNull()) {
				this->closeConnection(closeAbnormal);
			}
		}

	}
}

void JsonRpcWebsocketsConnection::sendNotification(const PreparedNtf& ntf) {
	this->sendTextMessage(ntf,true);
}

JsonRpcWebsocketsConnection::PreparedNtf JsonRpcWebsocketsConnection::prepareNotification(
		LightSpeed::ConstStrA name, LightSpeed::JSON::PNode arguments) {
	Synchronized<FastLock> _(lock);
	JSON::PNode req = json("method",name)
			("params",arguments)
			("id",nil);
	return  IRpcNotify::prepare(json.factory->toString(*req));
}

IRpcNotify *IRpcNotify::fromRequest(RpcRequest *r) {
	JsonRpcWebsocketsConnection *conn = JsonRpcWebsocketsConnection::getConnection(*r->httpRequest);
	return conn;
}

} /* namespace jsonsrv */

