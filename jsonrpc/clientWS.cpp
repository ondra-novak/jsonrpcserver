/*
 * clientWS.cpp
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#include "clientWS.h"

#include <lightspeed/base/actions/executor.h>
#include "../httpclient/httpClient.h"
#include "../httpserver/abstractWebSockets.tcc"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/containers/map.tcc"

#include "lightspeed/base/exceptions/netExceptions.h"

#include "lightspeed/base/actions/promise.tcc"

#include "../httpserver/IJobScheduler.h"
#include "rpcnotify.h"
#include "../httpserver/httpServer.h"
namespace jsonrpc {

ClientWS::ClientWS(const ClientConfig& cfg):WebSocketsClient(cfg),idcounter(1),reconnectMsg(0),reconnectDelay(1) {
	if (cfg.jsonFactory == null) jsonFactory = JSON::create();
	else jsonFactory = cfg.jsonFactory;

	cfgurl = cfg.url;
}


JSON::ConstValue ClientWS::onBackwardRPC(ConstStrA msg,
		const JSON::ConstValue params) {

	(void)msg;
	(void)params;

	throw jsonsrv::RpcCallError(THISLOCATION,404,"Method not found");
}

void ClientWS::connect(const ConnectConfig &config) {
	this->scheduler = config.scheduler;
	this->executor = config.executor;
	this->dispatcher = config.dispatcher;
	this->logObject = config.logObject;

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
				onNotify(method->getStringUtf8(), params);
			} else {
				try {
					JSON::ConstValue result = onBackwardRPC(method->getStringUtf8(), params);
					sendResponse(id, result, jsonFactory->newValue(null));
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
	try {
		Synchronized<FastLockR> _(lock);
		JSON::Value v = jsonFactory->fromString(msg);
		if (executor) executor->execute(IExecutor::ExecAction::create(this,&ClientWS::processMessage, v));
		else processMessage(v);
	} catch (...) {
		onParseError(msg);
	}
}


void ClientWS::onConnect() {
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
	Super::disconnect(reason);
	if (scheduler != null)
		scheduler->cancel(reconnectMsg,false);

}

ClientWS::~ClientWS() {
	if (scheduler != null)
		scheduler->cancelLoop(reconnectMsg);
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

}
