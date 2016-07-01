/*
 * clientWS.cpp
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */


#include "webSocketsClient.h"

#include <lightspeed/mt/notifier.h>
#include "../httpclient/httpClient.h"
#include "lightspeed/base/exceptions/httpStatusException.h"

#include "lightspeed/base/exceptions/netExceptions.h"

#include "lightspeed/base/actions/message.h"
namespace BredyHttpClient {

WebSocketsClient::WebSocketsClient(const ClientConfig& cfg):cfg(cfg){
}

WebSocketsClient::~WebSocketsClient() {
	disconnect(closeNormal);
}

void WebSocketsClient::sendTextMessage(ConstStrA msg) {
	Synchronized<FastLockR> _(lock);
	if (stream != null) try {
		Super::sendTextMessage(msg,true);
		return;
	} catch (NetworkException &e) {
		onReconnect().thenCall(Action::create(this,&WebSocketsClient::sendTextMessage, StringA(msg)));
		onCloseOutput(naturalNull);
	} else {
		onReconnect().thenCall(Action::create(this,&WebSocketsClient::sendTextMessage, StringA(msg)));
	}

}

void WebSocketsClient::sendBinMessage(ConstBin msg) {
	Synchronized<FastLockR> _(lock);
	if (stream != null) try {
		Super::sendBinMessage(msg,true);
		return;
	} catch (NetworkException &e) {
		onReconnect().thenCall(Action::create(this,&WebSocketsClient::sendBinMessage, StringB(msg)));
		onCloseOutput(naturalNull);
	} else {
		onReconnect().thenCall(Action::create(this,&WebSocketsClient::sendBinMessage, StringB(msg)));
	}

}

void WebSocketsClient::sendPing(ConstBin msg) {
	if (stream != null) {
		Super::sendPing(msg);
	}
}

void WebSocketsClient::sendPong(ConstBin msg) {
	if (stream != null) {
		Super::sendPong(msg);
	}
}

PNetworkStream WebSocketsClient::getStream() const {
	return stream;
}

void WebSocketsClient::onBinaryMessage(ConstBin ) {
}


void WebSocketsClient::onPong(ConstBin ) {
}


void WebSocketsClient::connectInternal() {
	HttpClient httpc(cfg);
	httpc.open(HttpClient::mGET, url);
	httpc.setHeader(httpc.fldUpgrade, "websocket");
	httpc.setHeader(httpc.fldConnection, "Upgrade");
	httpc.setHeader("Sec-WebSocket-Key", "x3JJHMbDL1EzLkh9GBhXDw==");
	stream = onInitConnection(httpc);
	this->listener->add(stream, this, INetworkResource::waitForInput, naturalNull, 0);
	onConnect();
}

void WebSocketsClient::connect(ConstStrA url, PNetworkEventListener listener) {
	Synchronized<FastLockR> _(lock);
	if (this->listener != null) {
		disconnect();
	}

	this->url = url;
	this->listener = listener.getMT();


	connectInternal();
}

PNetworkStream WebSocketsClient::onInitConnection(BredyHttpClient::HttpClient &http) {

	http.send();
	if (http.getStatus() != 101) {
		throw HttpStatusException(THISLOCATION,url,http.getStatus(), http.getStatusMessage());

	}
	//TODO: check headers to ensure, that ws connection is really established

	return http.getConnection();

}

void WebSocketsClient::disconnectInternal(natural reason) {

	if (stream != null) {
		if (reason != naturalNull) {
			try {
					closeConnection(reason);
				} catch(...) {
					//ignore errors - sending close packet is optional
				}
		}

		Notifier ntf;
		this->listener->remove(stream, this, &ntf);
		ntf.wait(null);
		stream = null;
	}
}

void WebSocketsClient::disconnect(natural reason) {
	Synchronized<FastLockR> _(lock);

	if (this->listener != null) {

		disconnectInternal(reason);
		listener = null;
	}
}


void WebSocketsClient::onConnect() {
	Synchronized<FastLockR> _(lock);
	while (!queuedMsgs.empty()) {
		Promise<void> r = queuedMsgs.top();
		queuedMsgs.pop();
		r.resolve();
	}
}

void WebSocketsClient::onLostConnection(natural code) {
	(void)code;
}

void WebSocketsClient::onTextMessage(ConstStrA ) {
}

void WebSocketsClient::onCloseOutput(natural code) {
	disconnectInternal(naturalNull);
	onLostConnection(code);
}

natural WebSocketsClient::stream_read(byte* buffer, natural length) {
	return stream->read(buffer,length);
}

void WebSocketsClient::stream_write(const byte* buffer, natural length) {
	stream->writeAll(buffer,length);
}

void WebSocketsClient::stream_closeOutput() {
	stream->closeOutput();
}

void WebSocketsClient::wakeUp(natural) throw () {
	rearmStream();
}

bool WebSocketsClient::reconnect() {
	Synchronized<FastLockR> _(lock);
	if (listener == null && url.empty())
		return false;
	connectInternal();
	return true;
}

bool WebSocketsClient::isDisconnected() const {
	return listener == null && url.empty();
}

void WebSocketsClient::rearmStream() {
	if (stream != null) {
		while (stream->dataReady()) {
			if (!onRawDataIncome()) onCloseOutput(naturalNull);
		}
		if (listener != null)
			listener->add(stream, this, INetworkResource::waitForInput, naturalNull, 0);
	}
}

Future<void> WebSocketsClient::onReconnect() {
	Future<void> r;
	queuedMsgs.push(r.getPromise());
	return r;
}

}

