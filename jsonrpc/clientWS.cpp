/*
 * clientWS.cpp
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#include "clientWS.h"

#include "../httpclient/httpClient.h"
#include "../httpserver/abstractWebSockets.tcc"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/containers/map.tcc"

#include "lightspeed/base/exceptions/netExceptions.h"

#include "lightspeed/base/actions/promise.tcc"
#include "rpcnotify.h"
namespace jsonrpc {

ClientWS::ClientWS(const ClientConfig& cfg):WebSocketsClient(cfg),idcounter(1) {
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

void ClientWS::onTextMessage(ConstStrA msg) {
	try {
		Synchronized<FastLockR> _(lock);
		JSON::Value v = jsonFactory->fromString(msg);
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
					onReceiveError(msg, errUnexpectedResponse);
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
				onReceiveError(msg,errInvalidFrame);
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
		onReceiveError(msg,errException);
	}
}


void ClientWS::onConnect() {
	Super::onConnect();
}

void ClientWS::onLostConnection(natural) {
	Synchronized<FastLockR> _(lock);
	waitingResults.clear();
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


}
