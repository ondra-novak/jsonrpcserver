/*
 * clientWS.cpp
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#include "clientWS.h"

#include <lightspeed/base/actions/executor.h>
#include <lightspeed/base/interface.tcc>

#include "../httpclient/httpClient.h"
#include "../httpserver/abstractWebSockets.tcc"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/containers/map.tcc"

#include "lightspeed/base/exceptions/netExceptions.h"

#include "lightspeed/base/actions/promise.tcc"

#include "../httpserver/IJobScheduler.h"
#include "rpcnotify.h"
#include "../httpserver/httpServer.h"
#include "errors.h"
#include "idispatch.h"
namespace jsonrpc {

using namespace json;

ClientWS::ClientWS(const ClientConfig& cfg):WebSocketsClient(cfg),idcounter(1),reconnectMsg(0),reconnectDelay(1),mepeer(this) {

	cfgurl = cfg.url;
}


void ClientWS::onIncomeRPC(JValue wholeMsg) {

	if (dispatcher) {
		WeakRef<IPeer> peer = mepeer;
		Future<JValue> res = dispatcher->dispatchMessage(wholeMsg,peer);
		res >> [peer](JValue res) {
			WeakRefPtr<IPeer> p(peer);
			if (p != null) {
				ClientWS *me = static_cast<ClientWS *>(p.get());
				me->sendResponse(res);
			}
		};

	} else {
		throw LookupException(THISLOCATION,"Client doesn't support dispatching");
	}
}


ConstStrA ClientWS::httpMethodStr("CLIENT");

class ClientWS::ClientFakeRequest: public BredyHttpSrv::IHttpRequestInfo {
public:
	ClientWS &owner;

	ClientFakeRequest(ClientWS &owner):owner(owner) {}

	virtual ConstStrA getMethod() const {return httpMethodStr;}
	virtual ConstStrA getPath() const {return owner.cfgurl;}
	virtual ConstStrA getProtocol() const {return "ws";}
	virtual HeaderValue getHeaderField(ConstStrA ) const {return HeaderValue();}
	virtual HeaderValue getHeaderField(HeaderField ) const {return HeaderValue();}
	virtual bool enumHeader(HdrEnumFn ) const {return false;}
	virtual ConstStrA getBaseUrl() const {return owner.cfgurl;}
	virtual StringA getAbsoluteUrl() const {return owner.cfgurl;}
	virtual StringA getAbsoluteUrl(ConstStrA ) const {return owner.cfgurl;}
	virtual bool keepAlive() const {return true;}
	virtual void beginIO() {}
	virtual void endIO() {}

};

void ClientWS::connect(const ConnectConfig &config) {
	this->scheduler = config.scheduler;
	this->executor = config.executor;
	this->dispatcher = config.dispatcher;
	this->logObject = config.logObject;
	if (this->dispatcher) this->fakeRequest = new ClientFakeRequest(*this);
	this->connectMethod = config.connectMethod;


	try {
		connect(config.listener);
	} catch (NetworkException &) {
		scheduleReconnect();
	}

}

void ClientWS::sendResponse(JValue resp) {

	Synchronized<FastLockR> _(lock);
	buffer.clear();
	resp.serialize([&](char c){buffer.add(c);});
	sendTextMessage(buffer);
}


void ClientWS::connect(PNetworkEventListener listener) {
	Super::connect(cfgurl,listener);
}

Future<IClient::Result> ClientWS::callAsync(StrViewA method, JValue params, JValue context) {
	Synchronized<FastLockR> _(lock);

	if (params == null) {
		params = JValue(json::array);
	} else if (params.type()!=json::array) {
		params = JValue({params});
	}

	natural thisid = idcounter++;
	JObject req;
	req("id",thisid)
		("method",method)
		("params",params);
	if (context.defined()) {
		req("context",context);
	}

	buffer.clear();
	JValue(req).serialize([&](char c){buffer.add(c);});
	sendTextMessage(buffer);
	Future<Result> f;
	waitingResults.insert(thisid,f.getPromise());



	return f;

}

IClient::Result ClientWS::call(StrViewA method, JValue params, JValue context) {
	return callAsync(method,params,context).getValue();
}

void ClientWS::processMessage(JValue v) {
	try {
		JValue result = v["result"];
		JValue error = v["error"];
		JValue context = v["context"];
		JValue method = v["method"];
		JValue id = v["id"];
		JValue params = v["params"];
		if (result.defined() || error.defined()) {
			if (id.defined() && !id.isNull()) {

				natural reqid = id.getUInt();
				const Promise<Result> *p = waitingResults.find(reqid);
				if (p == 0)
					onDispatchError(v);
				else {
					Promise<Result> q = *p;
					waitingResults.erase(reqid);
					if (p) {
						if (!error.defined()|| error.isNull()) {
							q.resolve(Result(result,context));
						} else {
							q.reject(RemoteException(THISLOCATION,error));
						}
					}
				}
			}//id=null - invalid frame
			else {
				onDispatchError(v);
			}
		} else if (method.defined() && params.defined()) {
			if (dispatcher == null) {
				onNotify(method.getString(), params, context);
			} else {
				onIncomeRPC(v);
			}
		}

	} catch (...) {
		onDispatchError(v);
	}

}

void ClientWS::onTextMessage(ConstStrA msg) {
	JValue v;
	try {
		v = JValue::fromString(StrViewA(msg));
	} catch (...) {
		onParseError(msg);
	}
	processMessage(v);
}


void ClientWS::onConnect() {
	if (dispatcher!=null && !connectMethod.empty()) {
		onNotify(connectMethod,JValue(json::array),JValue());
	}
	Super::onConnect();
}

void ClientWS::onLostConnection(natural) {
	Synchronized<FastLockR> _(lock);
	waitingResults.clear();
	if (scheduler != null)
			scheduleReconnect();
}


void ClientWS::sendNotify(StrViewA method, const JValue& params) {
	Synchronized<FastLockR> _(lock);
	sendNotify(PreparedNotify(method,params));
}

void ClientWS::sendNotify(const PreparedNotify& preparedNotify) {
	sendTextMessage(preparedNotify.content);
}

PreparedNotify ClientWS::prepareNotify(StrViewA method, const JValue& params) {
	Synchronized<FastLockR> _(lock);
	return PreparedNotify(method,params);
}

void ClientWS::disconnect(natural reason) {
	if (scheduler != null) {
		scheduler->cancel(reconnectMsg,false);
		scheduler = null;
	}

	Synchronized<FastLock> _(inExecutor);
	Super::disconnect(reason);

}

ClientWS::~ClientWS() {
	disconnect();
	mepeer.setNull();
}

bool ClientWS::reconnect() {
	try {
		return Super::reconnect();
	} catch (NetworkException &e) {
		scheduleReconnect();
		return true;
	} catch (IOException &e) {
		scheduleReconnect();
		return true;
	}
}

void ClientWS::scheduleReconnect() {
	natural d = reconnectDelay;
	reconnectDelay = (reconnectDelay * 3+1)/2;
	if (reconnectDelay > 60) reconnectDelay = 60;
	reconnectMsg = scheduler->schedule(
			ThreadFunction::create(this, &ClientWS::reconnect), d);
}


ClientWS::ConnectConfig ClientWS::ConnectConfig::fromHttpMapper( BredyHttpSrv::IHttpMapper& mapper) {
	//extract server from the mapper
	BredyHttpSrv::HttpServer &server = mapper.getIfc<BredyHttpSrv::HttpServer>();
	//retrieve TCP server instance
	TCPServer &tcp = server.getServer();
	ConnectConfig  out;
	//use TCP's server executor
	out.executor = tcp.getExecutor();
	//use TCP's server listener
	out.listener = tcp.getEventListener();
	//extract scheduler
	out.scheduler = mapper.getIfcPtr<BredyHttpSrv::IJobScheduler>();
	return out;
}

ClientWS::ConnectConfig ClientWS::ConnectConfig::fromRequest(BredyHttpSrv::IHttpRequestInfo& request) {
	//extract server from the mapper
	BredyHttpSrv::HttpServer &server = request.getIfc<BredyHttpSrv::HttpServer>();
	//retrieve TCP server instance
	TCPServer &tcp = server.getServer();
	ConnectConfig  out;
	//use TCP's server executor
	out.executor = tcp.getExecutor();
	//use TCP's server listener
	out.listener = tcp.getEventListener();
	//extract scheduler
	out.scheduler = request.getIfcPtr<BredyHttpSrv::IJobScheduler>();
	return out;
}

void ClientWS::wakeUp(natural reason) throw () {
	if (executor) {
		inExecutor.lock();
		executor->execute(IExecutor::ExecAction::create(this,&ClientWS::super_wakeUp,reason));
	} else {
		Super::wakeUp(reason);
	}
}

void ClientWS::super_wakeUp(natural reason) throw () {
	Super::wakeUp(reason);
	inExecutor.unlock();
}

void ClientWS::onNotify(StrViewA , const JValue& , const JValue& ) {

}

BredyHttpSrv::IHttpRequestInfo* ClientWS::getHttpRequest() const {
	return fakeRequest;
}

ConstStrA ClientWS::getName() const {
	return url;
}

natural ClientWS::getPortIndex() const {
	return 0;
}

IRpcNotify* ClientWS::getNotifySvc() const {
	return const_cast<ClientWS *>(this);

}

void ClientWS::setContext(Context* ctx) {
	ctxVar = ctx;
}

Context* ClientWS::getContext() const {
	return ctxVar;
}

IClient* ClientWS::getClient() const {
	return const_cast<ClientWS *>(this);
}

natural ClientWS::getVersion() const {
	return 0;
}

}

