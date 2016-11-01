/*
 * wsHandler.cpp
 *
 *  Created on: 3. 10. 2016
 *      Author: ondra
 */

#include "wsHandler.h"

#include "iclient.h"
#include "ipeer.h"
#include "lightspeed/base/debug/dbglog.h"

#include "errors.h"
#include "idispatch.h"
#include "lightspeed/base/actions/promise.tcc"
#include "rpcnotify.h"

#include <lightspeed/utils/json/jsonserializer.tcc>
#include <lightspeed/base/containers/avltree.tcc>

#include "httpHandler.h"

using LightSpeed::JSON::Serializer;
namespace jsonrpc {


WSHandler::WSHandler(IDispatcher& dispatcher)
	:dispatcher(dispatcher),events(0)
{

}



void WSHandler::setListener(IWSHandlerEvents* handler) {
	events = handler;
}

WeakRef<IWSHandlerEvents> WSHandler::getListener() const {
	return events;
}


class WSConnection: public BredyHttpSrv::WebSocketConnection, public IRpcNotify, public IClient, public IPeer {
public:
	WSConnection(IDispatcher &dispatcher,
			BredyHttpSrv::IHttpRequest&request, WeakRef<IWSHandlerEvents> events, natural version);
	~WSConnection();
	virtual void onConnect();
	virtual void onTextMessage(ConstStrA msg);
	virtual void onBinaryMessage(ConstBin msg);
	virtual void onCloseOutput(natural code);
	virtual void onPong(ConstBin msg);

	virtual PreparedNotify prepare(LightSpeed::ConstStrA name, JValue arguments);
	virtual void sendPrepared(const PreparedNotify &prepared, TimeoutControl tmControl = shortTimeout);
	virtual void sendNotification(LightSpeed::ConstStrA name, JValue arguments, TimeoutControl tmControl = standardTimeout);
	virtual void dropConnection();
	virtual void closeConnection(natural code);
	virtual Future<void> onClose();

	virtual Future<Result> callAsync(ConstStrA method, JValue params, JValue context = 0);

	virtual BredyHttpSrv::IHttpRequestInfo *getHttpRequest() const;
	virtual ConstStrA getName() const;
	virtual natural getPortIndex() const;
	virtual IRpcNotify *getNotifySvc() const;
	virtual void setContext(Context *ctx);
	virtual Context *getContext() const;
	virtual IClient *getClient() const;
	virtual natural getVersion() const {return version;}

	void processResponse(const JValue &req);
	void processRequest(const JValue &req);
	void sendResponse(const JValue &req);
	static void sendResponseAsync(const JValue &req, const WeakRef<IPeer> &peer);

	IDispatcher &dispatcher;
	BredyHttpSrv::IHttpRequestInfo &request;
	WeakRefTarget<IPeer> myPeer;
	Future<void> onCloseFuture;
	Optional<Promise<void> >onClosePromise;
	WeakRef<IWSHandlerEvents> events;
	ContextVar ctxVar;
	FastLockR lock;
	atomic nextPromiseId;

	AutoArrayStream<char> buffer;
	MicroLock bufferLock;

	typedef Map<natural, Promise<Result> > WaitingPromises;
	WaitingPromises waitingPromises;
	MicroLock promisesLock;

	natural version;

};



BredyHttpSrv::WebSocketConnection* WSHandler::onNewConnection(
		IRuntimeAlloc& alloc, BredyHttpSrv::IHttpRequest& request, ConstStrA vpath) {

	natural ver = HttpHandler::getVersionFromReq(request, vpath);

	return new(alloc) WSConnection(dispatcher,request, events, ver);

}



WSConnection::WSConnection(
		IDispatcher &dispatcher,
		BredyHttpSrv::IHttpRequest& request,
		WeakRef<IWSHandlerEvents> events, natural version)

:WebSocketConnection(request)
,dispatcher(dispatcher)
,request(request)
,myPeer(this)
,onCloseFuture(null)
,events(events)
,nextPromiseId(TimeStamp::now().asUnix())
,version(version)
{
}

WSConnection::~WSConnection() {
	//exit sequence
	WeakRefPtr<IWSHandlerEvents> eventPtr(events);
	//1. if events are defined, call onClose first.
	//   - ctxVar is still vallid, also peer
	if (eventPtr != null)
		eventPtr->onClose(WSContext(*this,dispatcher));
	//if closePromise was defined, resolve it now
	if (onClosePromise != null) {
		//resolve
		onClosePromise->resolve();
	}
	//now delete the context
	ctxVar = null;
	//invalidate peer
	myPeer.setNull();
	//destruction can continue
}

void WSConnection::onConnect() {
	WeakRefPtr<IWSHandlerEvents> eventPtr(events);
	if (eventPtr != null)
		eventPtr->onOpen(WSContext(*this,dispatcher));
}

void WSConnection::onTextMessage(ConstStrA msg) {


	JValue req = JValue::fromString(msg);
	if (req["result"] != null) {
		processResponse(req);
	} else {
		processRequest(req);
	}
}

void WSConnection::onBinaryMessage(ConstBin ) {
	closeConnection(closeAbnormal);
}

void WSConnection::onCloseOutput(natural) {
}

void WSConnection::onPong(ConstBin) {
}

PreparedNotify WSConnection::prepare(LightSpeed::ConstStrA name,JValue arguments) {

	class P: public PreparedNotify {
	public:
		P(StringA x):PreparedNotify(x){}
	};

	Synchronized<MicroLock> _(bufferLock);
	buffer.clear();
	JValue req = JObject("method", name)
			("params", arguments)
			("id",nullptr);

	req.serialize([&](char c){buffer.write(c);});
	return P(StringA(buffer.getArray()));


}


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


void WSConnection::sendPrepared(const PreparedNotify& prepared, TimeoutControl tmControl) {
	TmControlScope tmscope(this->stream,tmControl);
	sendTextMessage(prepared.content,true);
}

void WSConnection::sendNotification(LightSpeed::ConstStrA name,JValue arguments, TimeoutControl tmControl) {
	sendPrepared(prepare(name,arguments),tmControl);
}


void WSConnection::dropConnection() {
	stream->closeOutput();
}

void WSConnection::closeConnection(natural code) {
	WebSocketConnection::closeConnection(code);
}

Future<void> WSConnection::onClose() {
	Synchronized<FastLockR> _(lock);
	if (onClosePromise != null) {
		onCloseFuture.clear();
		onClosePromise = onCloseFuture.getPromise();
	}
	return onCloseFuture;
}

Future<IClient::Result> WSConnection::callAsync(ConstStrA method,
					JValue params, JValue context) {

	atomicValue promiseId = lockInc(nextPromiseId);
	JObject req;
	req("method",method)
			("params",params)
			("id",ToString<atomicValue>(promiseId));

	if (context != null) {
		req("context",context);
	}

	sendResponse(req);


	Future<Result> promise;
	{
		Synchronized<MicroLock> _(promisesLock);
		waitingPromises.insert(promiseId,promise.getPromise());
	}
	return promise;



}

BredyHttpSrv::IHttpRequestInfo* WSConnection::getHttpRequest() const {
	return &request;
}

ConstStrA WSConnection::getName() const {
	return request.getIfc<BredyHttpSrv::IHttpPeerInfo>().getPeerRealAddr();
}

natural WSConnection::getPortIndex() const {
	return request.getIfc<BredyHttpSrv::IHttpPeerInfo>().getSourceId();
}

IRpcNotify* WSConnection::getNotifySvc() const {
	return const_cast<IRpcNotify *>(static_cast<const IRpcNotify *>(this));
}

void WSConnection::setContext(Context* ctx) {
	ctxVar = ctx;
}

Context* WSConnection::getContext() const {
	return ctxVar;
}

IClient* WSConnection::getClient() const {
	return const_cast<IClient *>(static_cast<const IClient *>(this));
}

void WSConnection::processResponse(const JValue& req) {
	natural id = req["id"].getUInt();
	Optional<Promise<Result> > p;
	{
		Synchronized<MicroLock> _(bufferLock);
		const Promise<Result> *pres = waitingPromises.find(id);
		if (pres == 0) return;
		p = *pres;
		waitingPromises.erase(id);
	}
	if (req["error"].isNull()) {
		Result res(req["result"],req["context"]);
		p->resolve(res);
	}
	else
		p->reject(RemoteException(THISLOCATION,req["error"]));

}

void WSConnection::processRequest(const JValue& req) {
	Future<JValue> resp = dispatcher.dispatchMessage(req,myPeer);
	const JValue *js = resp.tryGetValue();
	if (js) {
		sendResponse(*js);
	} else {
		resp.thenCall(
				Message<void, JValue, void>::create(&WSConnection::sendResponseAsync,WeakRef<IPeer>(myPeer)));
	}
}

void WSConnection::sendResponse(const JValue& req) {
	Synchronized<MicroLock> _(bufferLock);
	buffer.clear();
	req.serialize([&](char c){buffer.write(c);});
	this->sendTextMessage(buffer.getArray(),true);
}

void WSConnection::sendResponseAsync(const JValue& req, const WeakRef<IPeer>& peer) {
	WeakRefPtr<IPeer> ptr(peer);
	if (ptr != null) {
		ptr->getIfc<WSConnection>().sendResponse(req);
	}
}

} /* namespace jsonrpc */
