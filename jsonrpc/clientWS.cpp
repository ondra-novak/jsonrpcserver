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
namespace jsonrpc {

ClientWS::ClientWS(ClientConfig& cfg):cfg(cfg),idcounter(1) {
	if (this->cfg.jsonFactory == null) {
		this->cfg.jsonFactory = JSON::create();
	}
}

ClientWS::~ClientWS() {
}



void ClientWS::onReceiveError(ConstStrA msg, bool exception) {
	(void)msg;
	(void)exception;
}

JSON::ConstValue ClientWS::onBackwardRPC(ConstStrA msg,
		const JSON::ConstValue params) {

	(void)msg;
	(void)params;

	throw jsonsrv::RpcCallError(THISLOCATION,404,"Method not found");
}


void ClientWS::sendResponse(JSON::ConstValue id, JSON::ConstValue result,
		JSON::ConstValue error) {

	Synchronized<FastLockR> _(callLock);
	JSON::Builder bld(cfg.jsonFactory);
	JSON::Builder::CObject req = bld("id",id)
			("result",result)
			("error",error);

	ConstStrA msg = cfg.jsonFactory->toString(*req);

	conn->sendTextMessage(msg,true);

}

void ClientWS::connectInternal() {
	BredyHttpClient::HttpClient httpc(cfg);
	httpc.open(BredyHttpClient::HttpClient::mGET, cfg.url);
	httpc.setHeader(httpc.fldUpgrade, "websocket");
	httpc.setHeader(httpc.fldConnection, "Upgrade");
	httpc.setHeader("Sec-WebSocket-Key", "x3JJHMbDL1EzLkh9GBhXDw==");
	stream = onInitConnection(httpc);
	conn = new WsConn(*this, stream.get());
	this->listener->add(stream, conn, INetworkResource::waitForInput,
			naturalNull, 0);
	onConnect();
}

void ClientWS::connect(INetworkEventListener* listener) {
	Synchronized<FastLockR> _(controlLock);
	if (this->listener != null) {
		disconnect();
	}

	this->listener = listener;


	connectInternal();
}

PNetworkStream ClientWS::onInitConnection(BredyHttpClient::HttpClient &http) {

	http.send();
	if (http.getStatus() != 200) {
		throw HttpStatusException(THISLOCATION,cfg.url,http.getStatus(), http.getStatusMessage());

	}
	//TODO: check headers to ensure, that ws connection is really established

	return http.getConnection();

}

void ClientWS::disconnectInternal() {
	if (failWait.isRunning())
		failWait.stop();

	if (stream != null) {
		Notifier ntf;
		this->listener->remove(stream, conn, &ntf);
		ntf.wait(null);
		conn = null;
		stream = null;
	}
}

void ClientWS::disconnect() {
	Synchronized<FastLockR> _(controlLock);

	if (this->listener != null) {

		disconnectInternal();
		listener = null;
	}
}

Future<IClient::Result> ClientWS::callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context) {
	Synchronized<FastLockR> _(callLock);
	JSON::Builder bld(cfg.jsonFactory);
	natural thisid = idcounter++;
	JSON::Builder::CObject req = bld("id",thisid)
			("method",method)
			("params",params);
	if (context != null) {
		req("context",context);
	}



	ConstStrA msg = cfg.jsonFactory->toString(*req);

	Future<Result> f;

	if (conn==null) {
			waitingRequests.push(Request(thisid,msg,f.getPromise()));
	} else {
		try {
			conn->sendTextMessage(msg,true);
			waitingResults.insert(thisid,f.getPromise());
		} catch (NetworkException &e) {
			waitingRequests.push(Request(thisid,msg,f.getPromise()));
			SyncReleased<FastLockR> _(callLock);
			onCloseOutput(naturalNull);
		}
	}

	return f;

}

IClient::Result ClientWS::call(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context) {
	return callAsync(method,params,context).getValue();
}

void ClientWS::onConnect() {
	Synchronized<FastLockR> _(controlLock);
	while (waitingRequests.empty()) {
		Request r = waitingRequests.top();
		waitingRequests.pop();
		try {
			conn->sendTextMessage(r.request,true);
			waitingResults.insert(r.id,r.result);
		} catch (NetworkException &e) {
			waitingRequests.push(r);
			SyncReleased<FastLockR> _(callLock);
			onCloseOutput(naturalNull);
			break;
		}
	}
}

void ClientWS::onLostConnection(natural code) {
	(void)code;
}

void ClientWS::onTextMessage(ConstStrA msg) {
	try {
		Synchronized<FastLockR> _(callLock);
		JSON::Value v = cfg.jsonFactory->fromString(msg);
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
				Promise<Result> q = *p;
				waitingResults.erase(reqid);
				if (p) {
					if (error == null || error->isNull()) {
						q.resolve(Result(result,context));
					} else {
						q.reject(RpcError(THISLOCATION,error));
					}
				}
			}//id=null - invalid frame
			else {
				onReceiveError(msg,false);
			}
		} else if (method != null && params != null) {
			if (id == null || id->isNull()) {
				onNotify(method->getStringUtf8(), params);
			} else {
				try {
					JSON::ConstValue result = onBackwardRPC(method->getStringUtf8(), params);
					sendResponse(id, result, cfg.jsonFactory->newValue(null));
				} catch (const RpcError &e) {
					sendResponse(id, cfg.jsonFactory->newValue(null), e.getError());
				} catch (const jsonsrv::RpcCallError &c) {
					RpcError e(THISLOCATION,cfg.jsonFactory,c.getStatus(),c.getStatusMessage());
					sendResponse(id, cfg.jsonFactory->newValue(null), e.getError());
				} catch (const std::exception &s) {
					RpcError e(THISLOCATION,cfg.jsonFactory,500,s.what());
					sendResponse(id, cfg.jsonFactory->newValue(null), e.getError());
				} catch (...) {
					RpcError e(THISLOCATION,cfg.jsonFactory,500,"fatal");
					sendResponse(id, cfg.jsonFactory->newValue(null), e.getError());
				}

			}
		}


	} catch (...) {
		onReceiveError(msg,true);
	}
}

void ClientWS::onCloseOutput(natural code) {
	Synchronized<FastLockR> _(controlLock);
	disconnectInternal();
	onLostConnection(code);
	failWait.start(ThreadFunction::create(this,&ClientWS::waitToReconnect));
}

natural ClientWS::WsConn::stream_read(byte* buffer, natural length) {
	return data->read(buffer,length);
}

void ClientWS::WsConn::stream_write(const byte* buffer, natural length) {
	data->writeAll(buffer,length);
}

void ClientWS::WsConn::stream_closeOutput() {
	data->closeOutput();
}

void ClientWS::WsConn::onTextMessage(ConstStrA msg) {
	owner.onTextMessage(msg);
}

void ClientWS::WsConn::onCloseOutput(natural code) {
	owner.onCloseOutput(code);
}

void ClientWS::WsConn::wakeUp(natural) throw () {
	if (!onRawDataIncome()) {
		onCloseOutput(naturalNull);
	}
}

void ClientWS::waitToReconnect() {
	if (Thread::sleep(1000)) {
		Synchronized<FastLockR> _(controlLock);
		connectInternal();
	}
}

}
