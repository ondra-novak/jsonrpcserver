/*
 * simpleHttpClient.cpp
 *
 *  Created on: Nov 21, 2012
 *      Author: ondra
 */

#include "simpleHttpClient.h"
#include "lightspeed/base/streams/netio.h"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/exceptions/netExceptions.h"
#include "lightspeed/base/text/textIn.tcc"
#include "lightspeed/base/exceptions/errorMessageException.h"

namespace jsonrpc {


SimpleHttpClient::SimpleHttpClient()
	:userAgent("Lightspeed application - default user agent")
	 ,proxyMode(pmDirect),noredirs(false),noexcepts(false),useHttp10(true)
	,curMode(mdIdle),readMode(rmUnlimited),resetHeaders(true),timeout(30000)
{

}

void SimpleHttpClient::setUserAgent(const String& uagent) {
	userAgent = uagent;
}

String SimpleHttpClient::getUserAgent() const {
	return userAgent;
}

void SimpleHttpClient::setProxy(ProxyMode md, String addr, natural port) {
	if (proxyMode != md || proxyAddr != addr || proxyPort != port)
		conn.unset();

	proxyMode = md;
	proxyAddr = addr;
	proxyPort = port;
}

SimpleHttpClient::ProxyMode SimpleHttpClient::getProxy() const {
	return proxyMode;
}

void SimpleHttpClient::getProxySettings(String& addr, natural& port) const {
	addr = proxyAddr;
	port = proxyPort;
}

void SimpleHttpClient::enableCookies(bool enable) {
	//unsupported
}

bool SimpleHttpClient::areCookiesEnabled() const {
	//unsupported
	return false;
}

natural SimpleHttpClient::getIOTimeout() const {
	return timeout;
}

void SimpleHttpClient::setIOTimeout(natural timeout) {
	this->timeout = timeout;
}

IHTTPStream& SimpleHttpClient::setMethod(ConstStrA method) {
	this->method = method;
	return *this;
}

IHTTPStream& SimpleHttpClient::setHeader(ConstStrA headerName,
		ConstStrA headerValue) {
	if (resetHeaders) {
		sendHdrs.clear();
		outHdrBuff.clear();
		resetHeaders = false;
	}
	sendHdrs.replace(outHdrBuff.add(headerName),outHdrBuff.add(headerValue));
	return *this;
}

IHTTPStream& SimpleHttpClient::cancel() {
	conn.unset();
	return *this;
}

StringA SimpleHttpClient::getReplyHeaders() {
	return inHdrBuff;
}

ConstStrA SimpleHttpClient::getHeader(ConstStrA field) {
	const ConstStrA *h = receivedHdrs.find(field);
	if (h == 0) return ConstStrA();
	else return *h;
}

bool SimpleHttpClient::enumHeaders(IEnumHeaders& enumHdr) {
	for (ReceivedHdrs::Iterator iter = receivedHdrs.getFwIter();iter.hasItems();) {
		const ReceivedHdrs::Entity &e = iter.getNext();
		if (enumHdr(e.key,e.value)) return true;
	}
	return false;

}

bool SimpleHttpClient::inConnected() const {
	return conn.isSet();
}

IHTTPStream& SimpleHttpClient::disableRedirect() {
	noredirs = true;
	return *this;
}

IHTTPStream& SimpleHttpClient::disableException() {
	noexcepts = true;
	return *this;
}

IHTTPStream& SimpleHttpClient::connect() {
	//TODO
	return *this;
}

natural SimpleHttpClient::getStatusCode() {
	return statusCode;
}

void SimpleHttpClient::setUrl(StringA url) {
	this->url = url;
	curHost = getHost(url);
	curPath = getPath(url);
}

bool SimpleHttpClient::extractChunkSize() {
	SeqTextInA textin(*conn);
	TextIn < SeqTextInA > scn(textin);
	scn.setWS(" \t");
	scn.setNL("\r\n");
	scn("\n%");
	if (scn(" %(+)[0-9A-Za-z]1 \n%")) {
		fragmentSize = scn[1].hex();
		if (fragmentSize == 0) {
			scn("\n%");
			return false;
		}
		return true;
	}
	reportReadException();
	return false;
}

natural SimpleHttpClient::read(void* buffer, natural size) {
	if (curMode != mdReading)
		sendRequest();

	natural s;
	switch (readMode) {
	case rmUnlimited:
		s = conn->getBufferHandle()->read(buffer, size);
		if (s == 0) {
			conn.unset();
			curMode = mdIdle;
		}
		return s;
	case rmSizeLimited:
		if (fragmentSize < size)
			size = fragmentSize;

		if (size == 0) {
			curMode = mdIdle;
			return 0;
		}
		s = conn->getBufferHandle()->read(buffer, size);
		if (s == 0) {
			conn.unset();
			curMode = mdIdle;
			reportReadException();
		}
		fragmentSize -= s;
		return s;
	case rmChunked:
		if (fragmentSize == 0) {
			if (!extractChunkSize()) {
				curMode = mdIdle;
				return 0;
			}
		}

		if (size > fragmentSize)
			size = fragmentSize;

		s = conn->getBufferHandle()->read(buffer, size);
		fragmentSize -= s;
		return s;
	}
	return 0;
}

natural SimpleHttpClient::write(const void* buffer, natural size) {
	ConstStringT < byte > data(reinterpret_cast<const byte*>(buffer), size);
	dataBuff.blockWrite(data, true);
	return size;
}

natural SimpleHttpClient::peek(void* buffer, natural size) const {
	SimpleHttpClient* mthis = const_cast<SimpleHttpClient*>(this);
	if (curMode != mdReading)
		mthis->sendRequest();

	natural s;
	switch (readMode) {
	case rmUnlimited:
		return mthis->conn->getBufferHandle()->peek(buffer, size);
	case rmSizeLimited:
		if (size == 0) {
			s = mthis->conn->getBufferHandle()->peek(buffer, size);
			if (s > fragmentSize)
				s = fragmentSize;

			return s;
		}
		if (size > fragmentSize)
			size = fragmentSize;

		if (size)
			return mthis->conn->getBufferHandle()->peek(buffer, size);
		else
			return 0;

	case rmChunked: {
		if (size == 0) {
			return mthis->conn->getBufferHandle()->peek(buffer, size);
		} else if (fragmentSize == 0) {
			if (!mthis->extractChunkSize()) {
				mthis->readMode = rmSizeLimited;
				return 0;
			} else
				return peek(buffer, size);
		}

		if (size > fragmentSize)
			size = fragmentSize;

		return mthis->conn->getBufferHandle()->peek(buffer, size);
	}
	}

	return 0;
}

bool SimpleHttpClient::canRead() const {
	if (!dataBuff.empty()) return true;
	if (!conn.isSet()) return true;
	byte b;
	switch(readMode) {
	case rmUnlimited: {
		return SeqFileInput(*conn).hasItems();
	}
	case rmSizeLimited: return fragmentSize>0;
	case rmChunked: return fragmentSize>0 || peek(&b,1) != 0;
	default: return false;
	}

}

bool SimpleHttpClient::canWrite() const {
	return true;
}

void SimpleHttpClient::flush() {
}

natural SimpleHttpClient::dataReady() const {
	if (canRead()) return  conn->dataReady();
	else return 0;
}

void SimpleHttpClient::skipBody() {
	while (curMode == mdReading) {
		byte buff[256];
		conn->blockRead(buff, 256, false);
	}
}

ConstStrA SimpleHttpClient::getHost(ConstStrA url) {
	natural pos = url.find(ConstStrA("://"));
	if (pos == naturalNull) throw NetworkResolveError(THISLOCATION,0, String(url));
	ConstStrA part = url.offset(pos+3);
	pos = part.find('/');
	if (pos != naturalNull) part = part.head(pos);
	pos = part.find('@');
	if (pos != naturalNull) part = part.offset(pos+1);
	return part;

}

ConstStrA SimpleHttpClient::getPath(ConstStrA url) {
	natural pos = url.find(ConstStrA("://"));
	if (pos == naturalNull) throw NetworkResolveError(THISLOCATION,0,String(url));
	ConstStrA part = url.offset(pos+3);
	pos = part.find('/');
	if (pos != naturalNull) return part.offset(pos);
	else return ConstStrA("/");

}

void SimpleHttpClient::sendRequest() {
	try {
		skipBody();
		reloadConnection();
		ConstStrA path = proxyUsed?ConstStrA(url):curPath;
		SeqTextOutA textstream(*conn);
		PrintTextA print(textstream);
		ConstStrA methodStr;
		if (method.empty()) {
			if (dataBuff.empty()) methodStr = "GET";
			else methodStr = "POST";
		} else {
			methodStr = method;
		}
		print.setNL("\r\n");
		print("%1 %2 HTTP/1.1\n") << methodStr << path;
		bool hasUserAgent = false;
		bool hasHost = false;
		for (SendHdrs::Iterator iter = sendHdrs.getFwIter(); iter.hasItems();) {
			const SendHdrs::Entity &e = iter.getNext();
			print("%1: %2\n") << e.key << e.value;
			if (e.key == ConstStrA("UserAgent")) hasUserAgent = true;
			if (e.key == ConstStrA("Host")) hasHost = true;
		}
		if (!hasUserAgent) print("UserAgent: %1\n")<< userAgent.getUtf8();
		if (!hasHost) print("Host: %1\n") << curHost;
		if (!dataBuff.empty() || method == "POST" || method == "PUT") {
			print("Content-Length: %1\n") << dataBuff.length();
		}
		print("\n");
		if (!dataBuff.empty()) conn->blockWrite(dataBuff.data(), dataBuff.length());
		resetHeaders = true;
		dataBuff.clear();
		receivedHdrs.clear();
		inHdrBuff.clear();
		bool lastNl = false;
		do {
			byte c = conn->getNext();
			if (c == '\n') {
				if (lastNl == true) break;
				else lastNl = true;
			} else if (c != '\r' ){
				lastNl = false;
			}
			inHdrBuff.add(c);
		} while (true);

		parseHeaders();
		curMode = mdReading;

		ConstStrA ta = getHeader("Transfer-Encoding");
		if (ta == "chunked") {
			readMode = rmChunked;
		} else {
			ConstStrA cl = getHeader("Content-Length");
			natural len;
			if (parseUnsignedNumber(cl.getFwIter(),len,10)) {
				readMode = rmSizeLimited;
				fragmentSize = len;
			} else {
				readMode = rmUnlimited;
			}
		}

		if (!noexcepts && statusCode/100 != 2) {
			skipBody();
			throw HttpStatusException(THISLOCATION,url,statusCode,ConstStrW());
		}
		firstRequest = false;
	} catch (...) {
		if (!firstRequest) {
			firstRequest=true;
			conn.unset();
			connAdr.clear();
			sendRequest();
		} else {
			Exception::rethrow(THISLOCATION);
			throw;
		}
	}


}

void SimpleHttpClient::reportReadException() {
	throw NetworkIOError(THISLOCATION,0,"HTTP Reading error");
}
/* namespace jsonrpc */

//NetworkAddress SimpleHttpClient::getTargetAddress(ConstStrA& path) {
void SimpleHttpClient::reloadConnection() {
	if (proxyMode == pmManual) {
		proxyUsed = true;
		StringA proxyA = proxyAddr.getUtf8();
		if (connAdr != proxyA) {
			connAdr = proxyA;
			conn.unset();
			NetworkAddress a(proxyA,proxyPort);
			NetworkStreamSource netsrc(a,1,timeout,timeout,StreamOpenMode::active);
			conn.init(netsrc.getNext());
			firstRequest = true;
		}
	} else {
		proxyUsed = false;
		if (url.head(7) != ConstStrA("http://"))
			throw ErrorMessageException(THISLOCATION, "Only Http protocol is supported by this client");
		if (curHost != connAdr) {
			firstRequest = true;
			connAdr = curHost;
			conn.unset();
			NetworkAddress a(curHost,80);
			NetworkStreamSource netsrc(a,1,timeout,timeout,StreamOpenMode::active);
			conn.init(netsrc.getNext());
		}
	}
}


static void cropLine(ConstStrA& hdrline) {
	while (!hdrline.empty() && isspace(hdrline.at(0)))
		hdrline = hdrline.offset(1);
	while (!hdrline.empty() && isspace(hdrline.at(hdrline.length() - 1)))
		hdrline = hdrline.crop(0, 1);
}

void SimpleHttpClient::parseHeaders() {
	TextParser<char, StaticAlloc<256> > parser;
	for (AutoArray<char>::SplitIterator iter = inHdrBuff.split('\n');
			iter.hasItems();) {
		ConstStrA hdrline = iter.getNext();
		cropLine(hdrline);
		if (parser("HTTP/%f1 %u2 %3",hdrline)) {
			statusCode = parser[2];
			receivedHdrs.clear();
		} else {
			natural div = hdrline.find(':');
			if (div != naturalNull) {
				ConstStrA hdr = hdrline.head(div);
				ConstStrA value = hdrline.offset(div + 1);
				cropLine(hdr);
				cropLine(value);
				receivedHdrs.replace(hdr, value);
			}
		}

	}


}

}

