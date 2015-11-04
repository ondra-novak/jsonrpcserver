/*
 * HTTPConn.cpp
 *
 *  Created on: 19. 10. 2015
 *      Author: ondra
 */

#include "HTTPConn.h"

#include <lightspeed/base/text/textParser.h>
#include <lightspeed/base/text/textstream.h>
#include "lightspeed/base/memory/staticAlloc.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/text/toString.tcc"
#include "lightspeed/base/text/textstream.tcc"
#include "lightspeed/base/actions/message.h"
#include "lightspeed/base/streams/fileiobuff.tcc"

using LightSpeed::parseUnsignedNumber;

namespace jsonrpc {


class ChunkedWriter: public INetworkStream {
public:
	ChunkedWriter(PNetworkStream next, const IAction &onclose):next(next),closed(false),onclose(onclose) {}
	~ChunkedWriter() {
		try {
			closeOutput();
		} catch (...) {

		}
	}


	virtual natural read(void *,  natural ) {return 0;}
	virtual natural peek(void *, natural ) const {return 0;}
	virtual bool canRead() const {return false;}
	virtual natural dataReady() const {return 0;}
	virtual natural write(const void *buffer,  natural size) {
		natural remain = chunk.getAllocatedSize() - chunk.length() - 2;
		if (remain == 0) {
			flush();
			return write(buffer,size);
		}
		if (size > remain) {
			return write(buffer,remain);
		}
		chunk.append(ConstBin(reinterpret_cast<const byte *>(buffer),size));
		return size;
	}
	virtual bool canWrite() const {return !closed;}
	virtual void flush() {
		closeChunk();
		next->flush();
	}
	virtual void closeOutput() {
		if (closed) return;
		closeChunk();
		sendEndChunk();
		flush();
		onclose();
	}
	virtual natural getDefaultWait() const {
		return waitForOutput;
	}
	virtual void setWaitHandler(WaitHandler *handler) {
		next->setWaitHandler(handler);
	}
	virtual WaitHandler *getWaitHandler() {
		return next->getWaitHandler();
	}

	virtual void setTimeout(natural time_in_ms) {
		next->setTimeout(time_in_ms);
	}
	virtual natural getTimeout() const  {
		return next->getTimeout();
	}
	virtual natural wait(natural waitFor, natural timeout) const {
		if (waitFor & waitForOutput) {
			if (chunk.length()+2 < chunk.getAllocatedSize()) return waitForOutput;
		}
		const_cast<ChunkedWriter *>(this)->closeChunk();
		return next->wait(waitFor,timeout);


	}
	virtual natural doWait(natural waitFor, natural timeout) const {
		if (waitFor & waitForOutput) {
			if (chunk.length()+2 < chunk.getAllocatedSize()) return waitForOutput;
		}
		const_cast<ChunkedWriter *>(this)->closeChunk();
		WaitHandler wh;
		return wh.wait(next,waitFor,timeout);
	}


protected:
	PNetworkStream next;
	AutoArray<byte, StaticAlloc<1500> > chunk;
	bool closed;

	void closeChunk() {
		if (chunk.empty()) return;
		AutoArray<char, StaticAlloc<100> > buffer;
		buffer.append(ToString<natural>(chunk.length(),16));
		buffer.add('\r');
		buffer.add('\n');
		next->writeAll(buffer.data(),buffer.length());
		chunk.add('\r');
		chunk.add('\n');
		next->writeAll(chunk.data(),chunk.length());
	}

	void sendEndChunk() {
		ConstStrA endchunk("0\r\n\r\n",5);
		next->writeAll(endchunk.data(),endchunk.length());
	}
	Action onclose;
};


class LimitReader: public INetworkStream {
public:
	LimitReader(PNetworkStream stream, natural limit):stream(stream), limit(limit) {}
	PNetworkStream stream;
	natural limit;

	virtual natural read(void *a,  natural b) {
		if (limit == 0) return 0;
		if (b > limit) b = limit;
		natural out = stream->read(a,b);
		limit-=out;
		return out;
		}
	virtual natural peek(void *a, natural b) const {
		if (limit == 0) return 0;
		if (b > limit) b = limit;
		return stream->peek(a,b);
	}
	virtual bool canRead() const {return limit?stream->canRead():false;}
	virtual natural dataReady() const {return limit?stream->dataReady():0;}
	virtual natural write(const void *,  natural ) {return 0;}
	virtual bool canWrite() const {return false;}
	virtual void flush() {}
	virtual void closeOutput() {}
	virtual natural getDefaultWait() const {return stream->getDefaultWait() & waitForInput;}
	virtual void setWaitHandler(WaitHandler *h) {stream->setWaitHandler(h);}
	virtual WaitHandler *getWaitHandler() {return stream->getWaitHandler();}

	virtual void setTimeout(natural x) {stream->setTimeout(x);}
	virtual natural getTimeout() const  {return stream->getTimeout();}
	virtual natural wait(natural a, natural b) const {return stream->wait(a,b);}
	virtual natural doWait(natural a, natural b) const {return WaitHandler().wait(stream,a,b);}
};

class ChunkedReader: public LimitReader {
public:
	ChunkedReader(PNetworkStream stream):LimitReader(stream,0),closed(false) {}
	bool closed;

	virtual natural read(void *a,  natural b) {
		if (closed) return 0;
		if (limit == 0) {
			openChunk();
			return read(a,b);
		}
		return LimitReader::read(a,b);
	}
	virtual natural peek(void *a, natural b) const {
		if (closed) return 0;
		if (limit == 0) {
			const_cast<ChunkedReader *>(this)->openChunk();
			return peek(a,b);
		}
		return LimitReader::peek(a,b);
	}
	virtual bool canRead() const {return !closed;}
	virtual natural dataReady() const {return limit?stream->dataReady():0;}
protected:
	void openChunk() {
		SeqFileInput in(stream);
		ScanTextA instr(in);
		while (isspace(instr.peek())) instr.skip();
		natural x;
		parseUnsignedNumber(instr,x,16);
		char a = instr.getNext();
		char b = instr.getNext();
		if (a != '\r' || b != '\n')
			throw HTTPReadException(THISLOCATION,1002,"Malformed chunked stream");
		if (x == 0) {
			char a = instr.getNext();
			char b = instr.getNext();
			if (a != '\r' || b != '\n')
				throw HTTPReadException(THISLOCATION,1002,"Malformed chunked stream");
			closed= true;
		} else {
			limit = x;
		}
	}

};



HTTPConn::HTTPConn(PNetworkStream stream):stream(stream),proxy(new LimitReader(stream,0)) {

}

void HTTPConn::openRequest(ConstStrA set_uri, ConstStrA set_method) {
	hdrfields.clear();
	requestHdr.clear();
	responseHdr.clear();
	statusMsg = ConstStrA();
	method = ConstStrA();
	uri = ConstStrA();

	uri = hdrfields.add(set_uri);
	method = hdrfields.add(set_method);
}

void HTTPConn::setHeader(ConstStrA field, ConstStrA value) {
	requestHdr.replace(hdrfields.add(field),hdrfields.add(value));
}

void HTTPConn::sendRequest() {
}

void HTTPConn::sendHeaders(PrintTextA &print) {
	print.setNL("\r\n");
	print("%1 %2 HTTP/1.1\n") << method << uri;
	for (HeaderMap::Iterator iter = requestHdr.getFwIter(); iter.hasItems();) {
		const HeaderMap::Entity& e = iter.getNext();
		print("%1: %2\n") << e.key << e.value;
	}
}

void HTTPConn::sendRequest(ConstStrA data) {
	if (method.empty()) {
		if (data.empty()) method=ConstStrA("GET");
		else method=ConstStrA("POST");
	}
	SeqFileOutput out(stream);
	PrintTextA print(out);

	sendHeaders(print);
	if (!data.empty()) {
		print("Content-Length: %1\n") << data.length();
	}
	print("\n");

	if (!data.empty()) stream->writeAll(data.data(),data.length());

	parseResponse();
}

void HTTPConn::sendRequestStreamBody() {
	if (method.empty()) {
		method=ConstStrA("GET");
	}
	SeqFileOutput out(stream);
	PrintTextA print(out);
	sendHeaders(print);
	print("Transfer-Encoding: chunked\n\n");
	proxy = new ChunkedWriter(stream, Action::create(this,&HTTPConn::parseResponse));


}

HeaderValue HTTPConn::getHeader(ConstStrA field) {

	const StringPoolA::Str *x = responseHdr.find(field);
	if (x) return HeaderValue(*x);
	return HeaderValue();

}

natural HTTPConn::getStatus() const {
	return status;
}

ConstStrA HTTPConn::getStatusMsg() const {
	return statusMsg;
}

natural HTTPConn::read(void* buffer, natural size) {
	return proxy->read(buffer,size);
}

natural HTTPConn::peek(void* buffer, natural size) const {
	return proxy->peek(buffer,size);

}

bool HTTPConn::canRead() const {
	return proxy->canRead();
}

natural HTTPConn::dataReady() const {
	return proxy->dataReady();
}

natural HTTPConn::write(const void* buffer, natural size) {
	return proxy->write(buffer,size);
}

bool HTTPConn::canWrite() const {
	return proxy->canWrite();
}

void HTTPConn::flush() {
	proxy->flush();
}

void HTTPConn::closeOutput() {
	proxy->closeOutput();
}

natural HTTPConn::getDefaultWait() const {
	return proxy->getDefaultWait();
}

void HTTPConn::setWaitHandler(WaitHandler* handler) {
	stream->setWaitHandler(handler);
}

INetworkStream::WaitHandler* HTTPConn::getWaitHandler() {
	return stream->getWaitHandler();
}

void HTTPConn::setTimeout(natural time_in_ms) {
	stream->setTimeout(time_in_ms);
}

natural HTTPConn::getTimeout() const {
	return stream->getTimeout();
}

natural HTTPConn::wait(natural waitFor, natural timeout) const {
	return proxy->wait(waitFor,timeout);
}

natural HTTPConn::doWait(natural waitFor, natural timeout) const {
	WaitHandler wh;
	return wh.wait(proxy,waitFor,timeout);
}

void HTTPConn::parseResponse() {
	SeqFileInBuff<1500> in(stream);
	ScanTextA scan(in);
	scan.setNL("\rn");

	if (!scan(" %u1 %2 HTTP/1.%3 \n%"))
		throw HTTPReadException(THISLOCATION,1000,"Malformed status line");

	while (!scan("\n%")) {
		if (!scan(" %1 : %2 \n%"))
			throw HTTPReadException(THISLOCATION,1001,"Malformed header field");
		StringPoolA::Str field = hdrfields.add(scan[1].str());
		StringPoolA::Str value = hdrfields.add(scan[2].str());

		responseHdr.replace(field,value);
	}

	HeaderValue te = getHeader("Transfer-Encoding");
	if (te.defined) {
		if (te == "chunked") proxy = new ChunkedReader(stream);
		else if (te == "identity") proxy = stream;
	}  else {
		proxy = stream;
	}

	HeaderValue cl = getHeader("Content-Length");
	if (cl.defined) {
		natural len = 0;
		parseUnsignedNumber(cl.getFwIter(),len);
		proxy = new LimitReader(proxy, len);
	}

}

} /* namespace jsonrpc */
