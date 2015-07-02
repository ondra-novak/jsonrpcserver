/*
 * jsonRpcWebsockets.cpp
 *
 *  Created on: 9.6.2015
 *      Author: ondra
 */

#include "jsonRpcWebsockets.h"

#include <lightspeed/base/interface.tcc>

#include "lightspeed/base/actions/promise.tcc"
namespace jsonsrv {

class JsonRpcWebsocketsConnection::HttpRequestWrapper: public BredyHttpSrv::IHttpRequest {
public:
	HttpRequestWrapper(BredyHttpSrv::IHttpRequest *r, JsonRpcWebsocketsConnection *owner):r(r),owner(owner) {}

	BredyHttpSrv::IHttpRequest  * const r;
	JsonRpcWebsocketsConnection * const owner;

protected:
	virtual ConstStrA getMethod() {return "WS";}
	virtual ConstStrA getPath() {return r->getPath();}
	virtual ConstStrA getProtocol() {return r->getProtocol();}
	virtual HeaderValue getHeaderField(ConstStrA field) const {return r->getHeaderField(field);}
	virtual HeaderValue getHeaderField(HeaderField field) const {return r->getHeaderField(field);}
	virtual bool enumHeader(HdrEnumFn fn) const {return r->enumHeader(fn);}
	virtual void sendHeaders() {}
	virtual void header(ConstStrA field, ConstStrA value) {}
	virtual void header(HeaderField field, ConstStrA value) {}
	virtual void status(natural code, ConstStrA msg = ConstStrA()) {}
	virtual void errorPage(natural code, ConstStrA msg = ConstStrA(), ConstStrA expl = ConstStrA()) {}
	virtual void redirect(ConstStrA url, int code = 0) {}
	virtual ConstStrA getBaseUrl() const {return r->getBaseUrl();}
	virtual void useHTTP11(bool use) {}
	virtual bool isField(ConstStrA text, HeaderField fld) const {return r->isField(text,fld);}
	virtual natural read(void *buffer,  natural size) {return 0;}
    virtual natural write(const void *buffer,  natural size)  {return 0;}
	virtual natural peek(void *buffer, natural size) const {return 0;}
	virtual bool canRead() const {return false;}
	virtual bool canWrite() const {return false;}
	virtual void flush() {}
	virtual natural dataReady() const {return 0;}
	virtual void finish() {}
	virtual bool headersSent() const {return true;}
	virtual natural callHandler(ConstStrA path, IHttpHandler **h = 0) {return 0;}
	virtual natural callHandler(IHttpRequest &request, IHttpHandler **h = 0) {return 0;}
	virtual natural forwardRequest(ConstStrA path) {return 0;}
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
	virtual void setMaxPostSize(natural bytes) {}
	virtual void attachThread(natural status) {}
	virtual void closeOutput() {}

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

Promise<JSON::PNode> JsonRpcWebsocketsConnection::callMethod(ConstStrA name, JSON::PNode arguments) {
	Synchronized<FastLock> _(lock);
	natural promiseId = nextPromiseId++;
	JSON::PNode req = json("method",name)
			("params",arguments)
			("id",promiseId);
	ConstStrA msg = json.factory->toString(*req);
	this->sendTextMessage(msg,true);

	PromiseResolution<JSON::PNode> result;
	Promise<JSON::PNode> promise(result);
	waitingPromises.insert(promiseId,result);
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
	JSON::PNode req = json.factory->fromString(msg);
	if (req->getPtr("result")) {
		natural id = req["id"]->getUInt();
		const PromiseResolution<JSON::PNode> *pres = waitingPromises.find(id);
		if (pres == 0) return;
		PromiseResolution<JSON::PNode> p = *pres;
		waitingPromises.erase(id);
		if (req["error"]->isNull()) p.resolve(req["result"]);
		else p.reject(RpcError(THISLOCATION,req["error"]));
	} else {
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
	}

}
JsonRpcWebsockets::JsonRpcWebsockets(IJsonRpc& handler,StringA openMethod):handler(handler),openMethod(openMethod.getMT()) {
}

WebSocketConnection* JsonRpcWebsockets::onNewConnection(
		IRuntimeAlloc& alloc, IHttpRequest& request,
		ConstStrA vpath) {

	return new(alloc) JsonRpcWebsocketsConnection(request,handler,openMethod);
}

void JsonRpcWebsocketsConnection::onCloseOutput(natural unsignedLongInt) {
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

} /* namespace jsonsrv */

