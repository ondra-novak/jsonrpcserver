#include "webSocketsClient.h"
#include "lightspeed/base/exceptions/httpStatusException.h"

namespace BredyHttpSrv {

WebSocketClientConnection::WebSocketClientConnection(PNetworkStream &stream)
	:stream(stream), connected(false)
{
	
}

Promise<void> WebSocketClientConnection::connectToServer(ConstStrA host, ConstStrA path, ConstStrA method /*= ConstStrA()*/, ConstStringT<HeaderLine> headers /*= ConstStringT<HeaderLine>()*/)
{
	if (connected) {
		throw AlreadyConnected(THISLOCATION);
	}

	Promise<void> retval;
	connectResolution = retval.createResult();
	
	SeqTextOutA textOut(stream);
	PrintTextA print(textOut);
	print.setNL("\r\n");

	if (method.empty()) method = "GET";
	if (path.empty()) path = "/";	

	print("%1 %2 HTTP/1.1\n") << method << path;
	print("Host: %1\n") << host;
	print("Upgrade: websocket\n");
	print("Connection: upgrade\n");
	print("Sec-WebSocket-Key: %1\n") << createWSKey();

	for (natural i = 0; i < headers.length(); i++) {
		print("%1: %2\n") << headers[i].field << headers[i].value;
	}

	return retval;
}



bool WebSocketClientConnection::processResponse(natural sep)
{
	ConstStrA hdrstr(reinterpret_cast<const char *>(fragmentBuffer.data()), sep);
	TextIn<ConstStrA::Iterator, SmallAlloc<256> > rd(hdrstr.getFwIter());
	rd.setNL("\r\n");

	if (rd("HTTP/1.1\b%u1\b%2 \n%")) {

		status = rd[1];
		StringA statusMsg = rd[2].str();

		while (!rd("\n")) {
			if (rd(" %1 : %2 \n%")) {
				hdrs.replace(strpool.add(rd[1].str()), strpool(rd[2].str()));
			}
		}

		if (status != 101) {
			connectResolution->reject(HttpStatusException(THISLOCATION, String(), status, statusMsg));
		}

	}
	return false;
}

void WebSocketClientConnection::onConnect()
{

}

void WebSocketClientConnection::onTextMessage(ConstStrA msg)
{

}

void WebSocketClientConnection::onBinaryMessage(ConstBin msg)
{

}

void WebSocketClientConnection::onCloseOutput(natural code)
{

}

void WebSocketClientConnection::onPong(ConstBin msg)
{

}

bool WebSocketClientConnection::onRawDataIncome()
{
	if (connected) {
		return Super::onRawDataIncome();
	}
	else {

		stream.getBuffer().fetch();
		natural curlen = fragmentBuffer.length();
		natural rddata = stream.dataReady();
		fragmentBuffer.resize(curlen + rddata);
		stream.blockRead(fragmentBuffer.data() + curlen, rddata);;
		natural startsrch = curlen > 4 ? curlen - 4 : 0;
		natural foundend = fragmentBuffer.find(ConstBin(
			reinterpret_cast<const byte *>("\r\n\r\n"), 4), startsrch);
		if (foundend != naturalNull)
			return processResponse(foundend+4);
		else
			return true;
	}
}


}