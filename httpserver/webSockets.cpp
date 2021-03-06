/*
 * webSockets.cpp
 *
 *  Created on: 30. 4. 2014
 *      Author: ondra
 */

#include <stdlib.h>
#include <string.h>
#include "webSockets.h"
#include <lightspeed/base/containers/convertString.h>
#include "lightspeed/utils/base64.h"
#include "lightspeed/base/debug/dbglog.h"
#include "sha1.h"
#include "lightspeed/base/framework/ITCPServer.h"
#include "lightspeed/base/interface.tcc"
#include "abstractWebSockets.tcc"


namespace BredyHttpSrv {

//template class AbstractWebSocketConnection<WebSocketConnection, true>;


natural AbstractWebSocketsHandler::onRequest(IHttpRequest& request,
		ConstStrA vpath) {

	ITCPServerConnControl &connControl = request.getIfc<ITCPServerConnControl>();


	HeaderValue upgrade = request.getHeaderField(IHttpRequest::fldUpgrade);
	HeaderValue connection = request.getHeaderField(IHttpRequest::fldConnection);
	if (upgrade != "websocket" || (connection.find(ConstStrA("Upgrade")) == naturalNull && connection.find(ConstStrA("upgrade")) == naturalNull))
		return 0;
	HeaderValue secKey = request.getHeaderField("Sec-WebSocket-Key");

	WebSocketConnection *websoc = onNewConnection(StdAlloc::getInstance(), request, vpath);

	if (websoc == 0)
		return stReject;

	request.setConnectionContext(websoc);

	request.status(101, "Switching Protocols");
	request.header(IHttpRequest::fldUpgrade, upgrade);
	request.header(IHttpRequest::fldConnection, connection);

	StringA testStr = secKey + ConstStrA("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	SHA1 sha1;
	sha1.update(std::string(testStr.data(), testStr.length()));
	std::string result = sha1.final();
	ConstStrA sha1res(result.data(), result.length());

	StringA sha1res_base64 = convertString(Base64EncoderChr(), sha1res);
	request.header("Sec-WebSocket-Accept", sha1res_base64);
	ConstStrA protoName = websoc->getProtocolName();
	if (!protoName.empty()) {
		request.header("Sec-WebSocket-Protocol", protoName);

	}

	request.sendHeaders();
	websoc->onConnect();

	connControl.setDataReadyTimeout(naturalNull);

	return stContinue;

}

natural AbstractWebSocketsHandler::onData(IHttpRequest& request) {
	WebSocketConnection *handler =
			dynamic_cast<WebSocketConnection *>(request.getConnectionContext());
	if (handler) {
		try {
			if (handler->onRawDataIncome())
				return stContinue;
		} catch (std::exception &e) {
			handler->closeConnection(WebSocketsConstants::closeInternalServerError);
			LS_LOG.error("Unhandled exception in websocket handler: %1") << e.what();
		}
		request.setConnectionContext(0);
	}
	return stOK;
}


void WebSocketConnection::onConnect() {}

void WebSocketConnection::onCloseOutput(natural ) {}

void WebSocketConnection::onPong(ConstBin ) {}

void WebSocketConnection::onTextMessage(ConstStrA ) {
}

void WebSocketConnection::onBinaryMessage(ConstBin ) {
}


natural WebSocketConnection::stream_read(byte* buffer, natural length) {
	return stream->read(buffer,length);
}

void WebSocketConnection::stream_write(const byte* buffer, natural length) {
	stream->writeAll(buffer,length);
}

void WebSocketConnection::stream_closeOutput() {
	stream->closeOutput();
}




WebSocketConnection::WebSocketConnection(IHttpRequest& request) {
	stream = request.getConnection();
}

template void AbstractWebSocketConnection<WebSocketConnection, true>::sendTextMessage(ConstStrA, bool);
template void AbstractWebSocketConnection<WebSocketConnection, true>::sendBinMessage(ConstBin, bool);


} /* namespace BredyHttpSrv */
