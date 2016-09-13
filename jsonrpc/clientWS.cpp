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
#include "ijsonrpc.h"
namespace jsonrpc {

ClientWS::ClientWS(const ClientConfig& cfg):WebSocketsClient(cfg),idcounter(1),reconnectMsg(0),reconnectDelay(1) {
	if (cfg.jsonFactory == null) jsonFactory = JSON::create();
	else jsonFactory = cfg.jsonFactory;

	cfgurl = cfg.url;
}


void ClientWS::onIncomeRPC(ConstStrA msg,
		const JSON::Value &params, const JSON::Value &context, const JSON::Value &id) {

	if (dispatcher) {
		jsonsrv::IJsonRpc::CallResult res =  dispatcher->callMethod(fakeRequest,msg,params,context,id);
		if (logObject) logObject->logMethod(cfgurl,msg,params,context,res.logOutput);
		sendResponse(id,res.result,res.error);
	} else {
		throw jsonsrv::RpcCallError(THISLOCATION,404,"Method not found");
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

void ClientWS::sendResponse(JSON::ConstValue id, JSON::ConstValue result,
		JSON::ConstValue error) {

	Synchronized<FastLockR> _(lock);
	JSON::Builder bld(jsonFactory);
	JSON::Builder::CObject req = bld("id",id)
			("result",result)
			("error",error);

	ConstStrA msg = jsonFactory->toString(*req);

	sendTextMessage(msg);
}


void ClientWS::connect(PNetworkEventListener listener) {
	Super::connect(cfgurl,listener);
}

Future<IClient::Result> ClientWS::callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context) {
	Synchronized<FastLockR> _(lock);

	if (params == null) {
		params = jsonFactory->array();
	} else if (!params->isArray()) {
		params = JSON::Container(jsonFactory->array()).add(params);
	}

	JSON::Builder bld(jsonFactory);
	natural thisid = idcounter++;
	JSON::Builder::CObject req = bld("id",thisid)
			.container()
			("method",method)
			("params",params);
	if (context != null) {
		req("context",context);
	}



	ConstStrA msg = jsonFactory->toString(*req);
	Future<Result> f;
	sendTextMessage(msg);
	waitingResults.insert(thisid,f.getPromise());

	return f;

}

IClient::Result ClientWS::call(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context) {
	return callAsync(method,params,context).getValue();
}

void ClientWS::processMessage(JSON::Value v) {
	try {
		JSON::Value result = v["result"];
		JSON::Value error = v["error"];
		JSON::Value context = v["context"];
		JSON::Value method = v["method"];
		JSON::Value id = v["id"];
		JSON::Value params = v["params"];
		if (result != null || error != null) {
			if (id != null && !id->isNull()) {

				natural reqid = id->getUInt();
				const Promise<Result> *p = waitingResults.find(reqid);
				if (p == 0)
					onDispatchError(v);
				else {
					Promise<Result> q = *p;
					waitingResults.erase(reqid);
					if (p) {
						if (error == null || error->isNull()) {
							q.resolve(Result(result,context));
						} else {
							q.reject(RpcError(THISLOCATION,error));
						}
					}
				}
			}//id=null - invalid frame
			else {
				onDispatchError(v);
			}
		} else if (method != null && params != null) {
			if (id == null || id->isNull()) {
				onNotify(method->getStringUtf8(), params, context);
			} else {
				try {
					onIncomeRPC(method->getStringUtf8(), params,context,id);
				} catch (const RpcError &e) {
					sendResponse(id, jsonFactory->newValue(null), e.getError());
				} catch (const jsonsrv::RpcCallError &c) {
					RpcError e(THISLOCATION,jsonFactory,c.getStatus(),c.getStatusMessage());
					sendResponse(id, jsonFactory->newValue(null), e.getError());
				} catch (const std::exception &s) {
					RpcError e(THISLOCATION,jsonFactory,500,s.what());
					sendResponse(id, jsonFactory->newValue(null), e.getError());
				} catch (...) {
					RpcError e(THISLOCATION,jsonFactory,500,"fatal");
					sendResponse(id, jsonFactory->newValue(null), e.getError());
				}

			}
		}

	} catch (...) {
		onDispatchError(v);
	}

}

void ClientWS::onTextMessage(ConstStrA msg) {
	JSON::Value v;
	try {
		v = jsonFactory->fromString(msg);
	} catch (...) {
		onParseError(msg);
	}
	processMessage(v);
}


void ClientWS::onConnect() {
	if (dispatcher!=null && !connectMethod.empty()) {
		onNotify(connectMethod,jsonFactory->array(),null);
	}
	Super::onConnect();
}

void ClientWS::onLostConnection(natural) {
	Synchronized<FastLockR> _(lock);
	waitingResults.clear();
	if (scheduler != null)
			scheduleReconnect();
}


void ClientWS::sendNotify(ConstStrA method, const JSON::ConstValue& params) {
	Synchronized<FastLockR> _(lock);
	sendNotify(PreparedNotify(method,params,jsonFactory));
}

void ClientWS::sendNotify(const PreparedNotify& preparedNotify) {
	sendTextMessage(preparedNotify.content);
}

PreparedNotify ClientWS::prepareNotify(ConstStrA method, const JSON::ConstValue& params) {
	Synchronized<FastLockR> _(lock);
	return PreparedNotify(method,params,jsonFactory);
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

ClientWS::ConnectConfig ClientWS::ConnectConfig::fromRequest(BredyHttpSrv::IHttpRequest& request) {
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

void ClientWS::onNotify(ConstStrA method, const JSON::Value& params, const JSON::Value& context) {
	if (dispatcher) {
		jsonsrv::IJsonRpc::CallResult res =  dispatcher->callMethod(fakeRequest,method,params,context,JSON::getConstant(JSON::constNull));
		if (logObject) logObject->logMethod(cfgurl,method,params,context,res.logOutput);
	}

}



}

