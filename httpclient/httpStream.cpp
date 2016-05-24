/*
 * HttpStream.cpp
 *
 *  Created on: Jan 20, 2016
 *      Author: ondra
 */

#include <lightspeed/base/interface.tcc>

#include <lightspeed/base/namedEnum.tcc>
#include <lightspeed/base/streams/fileiobuff_ifc.h>
#include <lightspeed/base/text/textstream.tcc>
#include "lightspeed/base/containers/map.tcc"
#include "httpStream.h"

namespace BredyHttpClient {

HttpRequest::HttpRequest(IOutputStream *com, ConstStrA path,ConstStrA method, bool useHTTP11)
:bodyHandling(useHTTP11?useChunked:useBuffered),inBody(false),print(SeqFileOutput(com)),com(*com)
,chunkMinSize(defaultChunkMinSize),chunkMaxSize(defaultChunkMaxSize),outputClosed(false)

{
	if (com == 0) throwNullPointerException(THISLOCATION);
	initRequest(path,method,useHTTP11);
}
HttpRequest::HttpRequest(IOutputStream *com, ConstStrA path,Method method, bool useHTTP11)
:bodyHandling(useHTTP11?useChunked:useBuffered),inBody(false),print(SeqFileOutput(com)),com(*com)
,chunkMinSize(defaultChunkMinSize),chunkMaxSize(defaultChunkMaxSize),outputClosed(false)

{
	if (com == 0) throwNullPointerException(THISLOCATION);
	initRequest(path,getMethodName(method),useHTTP11);
}


void HttpRequest::initRequest(ConstStrA path,ConstStrA method, bool useHTTP11) {
	print.setNL("\r\n");
	print("%1 %2 HTTP/1.%3\n") << method << path << (useHTTP11?1:0);
	StrCmpCI<char> cmp;
	needBody = cmp(method,getMethodName(mPOST)) == cmpResultEqual
				|| cmp(method,getMethodName(mPUT)) == cmpResultEqual;
	setContentLength(unknownLength);
	canChunked = useHTTP11;
}

HttpRequest::~HttpRequest() {
}

bool HttpRequest::headersSent() const {
	return inBody;
}

natural HttpRequest::write(const void* buffer, natural size) {
	if (!inBody) beginBody();
	natural out;
	ConstBin data(reinterpret_cast<const byte *>(buffer),size);
	switch (bodyHandling) {
	case useRemainLength:
		if (size > remainLength) size = remainLength;
		if (size == 0) return 0;
		out = com.write(buffer,size);
		remainLength -= out;
		if (remainLength == 0) outputClosed = true;
		return out;
	case useChunked:
		if (size >= chunkMinSize && (chunk.empty() || chunk.length() >= chunkMinSize)) {
			if (!chunk.empty()) flushChunk();
			writeChunk(data);
		} else {
			chunk.append(data);
			if (chunk.length() >= chunkMaxSize) flushChunk();
		}
		return size;
	case useBuffered:
		chunk.append(data);
		return size;
	case useDefinedByUser:
		return com.write(buffer,size);
	}
}

natural HttpRequest::defaultChunkMinSize = 4096;
natural HttpRequest::defaultChunkMaxSize = 65536;

natural HttpRequest::getChunkMaxSize() const {
	return chunkMaxSize;
}

void HttpRequest::setChunkMaxSize(natural chunkMaxSize) {
	this->chunkMaxSize = chunkMaxSize;
}

natural HttpRequest::getChunkMinSize() const {
	return chunkMinSize;
}

void HttpRequest::setChunkMinSize(natural chunkMinSize) {
	this->chunkMinSize = chunkMinSize;
}

IOutputStream& HttpRequest::getCom()  {
	return com;
}

natural HttpRequest::getDefaultChunkMaxSize()  {
	return defaultChunkMaxSize;
}

void HttpRequest::setDefaultChunkMaxSize(natural defaultChunkMaxSize) {
	HttpRequest::defaultChunkMaxSize = defaultChunkMaxSize;
}

natural HttpRequest::getDefaultChunkMinSize(){
	return defaultChunkMinSize;
}

void HttpRequest::setDefaultChunkMinSize(natural defaultChunkMinSize) {
	this->defaultChunkMinSize = defaultChunkMinSize;
}


bool HttpRequest::canWrite() const {
	return !outputClosed;
}

void HttpRequest::flush() {
	if (!inBody || !canWrite()) return;
	if (bodyHandling == useChunked) flushChunk();
	if (bodyHandling != useBuffered) com.flush();

}

void HttpRequest::closeOutput() {
	if (!inBody) beginBody();
	outputClosed = true;
	switch (bodyHandling) {
	case useRemainLength:
		if (remainLength != 0) {
			com.closeOutput();
			throw; //ContentIsShorterThenDeclaredException(THISLOCATION, remainLength)
		}
		break;
	case useChunked:
		flushChunk(); //close current chunk
		writeChunk(ConstBin()); //write empty chunk
		break;
	case useBuffered:
		if (!needBody) {
			print("\n");
		} else {
			print("%1: %2\n\n") << getHeaderFieldName(fldContentLength) << chunk.length();
			com.writeAll(chunk.data(), chunk.length());
		}
		break;
	case useDefinedByUser:
		break;
	}
	com.flush();
}

void HttpRequest::setContentLength(natural length) {
	if (inBody) throw ;//HeadersAlreadySentException(THISLOCATION);
	if (length == unknownLength) {
		if (needBody && canChunked) bodyHandling = useChunked;
		else bodyHandling = useBuffered;
	} else if (length == calculateLength) {
		bodyHandling = useBuffered;
	} else {
		bodyHandling = useRemainLength;
		remainLength = length;
	}
}

void HttpRequest::setHeader(Field hdrField, ConstStrA value) {
	setHeader(getHeaderFieldName(hdrField), value);
}

void HttpRequest::setHeader(ConstStrA hdrField, ConstStrA value) {
	if (inBody) throw ;//HeadersAlreadySentException(THISLOCATION);
	StrCmpCI<char> cmp;
	if (cmp(hdrField, getHeaderFieldName(fldContentLength)) == cmpResultEqual
			|| cmp(hdrField, getHeaderFieldName(fldTransferEncoding)) == cmpResultEqual) {
		bodyHandling = useDefinedByUser;
	}
	print("%1: %2\n") << hdrField << value;
}

void HttpRequest::beginBody() {
	if (inBody) throw ;//HeadersAlreadySentException(THISLOCATION);
	inBody = true;
	switch (bodyHandling) {
		//we don't care about length - set length to some number not equal to zero
	case useDefinedByUser: remainLength = 100;
							print("\n");
							break;
		//write content length, terminate header block and open stream
	case useRemainLength: print("%1: %2\n\n") << getHeaderFieldName(fldContentLength) << remainLength;break;
		//declare transfer encoding and finish header block
	case useChunked: print("%1: %2\n\n") << getHeaderFieldName(fldTransferEncoding) << "chunked";
		//do nothing, keep headers opened, it need to write Content-Length at the end
	case useBuffered: break;
	}
}

void HttpRequest::flushChunk() {
	if (chunk.empty()) return;
	writeChunk(chunk);
	chunk.clear();
}

void HttpRequest::writeChunk(ConstBin data) {
	print("%1\n") << setBase(16) << data.length();
	com.writeAll(data.data(),data.length());
	print("\n");
}


HttpResponse::HttpResponse(IInputStream* com, IHttpResponseCB &cb)
:com(*com),hdrCallback(cb),rMode(rmStatus),ibuff(com->getIfc<IInputBuffer>()),hdrContentLength(naturalNull),hdrTEChunked(false) {
	if (com == 0) throwNullPointerException(THISLOCATION);

}

HttpResponse::HttpResponse(IInputStream* com, IHttpResponseCB &cb, ReadHeaders)
:com(*com),hdrCallback(cb),rMode(rmStatus),ibuff(com->getIfc<IInputBuffer>()),hdrContentLength(naturalNull),hdrTEChunked(false) {
	if (com == 0) throwNullPointerException(THISLOCATION);
	readHeaders();
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::readHeaders() {
	natural p = checkStream();
	while (rMode == rmStatus || rMode == rmHeaders || rMode == rmChunkHeader) {
		com.canRead(); //block if data are not yet available
		p = checkStream();
	}
}

void HttpResponse::waitAfterContinue() {
	com.canRead(); //block if data are not yet available;
	checkStream();
}

void HttpResponse::waitAfterContinue(ReadHeaders) {
	com.canRead(); //block if data are not yet available;
	readHeaders();
}


bool HttpResponse::isKeepAliveEnabled() const {
	return keepAlive;
}

natural HttpResponse::getContentLength() const {
	return remainLength;
}



bool HttpResponse::checkStream() {
	switch (rMode) {
		case rmContinue:
			//if data not ready, we still waiting for continue, return 1 for EOF
			if (com.dataReady() == 0) return true;
			//otherwise read status line of new header
			else {
				readHeaderLine(tohStatusLine);
				if (com.dataReady()) return checkStream();
				return false;
			}
		case rmStatus:
			readHeaderLine(tohStatusLine);
			if (com.dataReady()) return checkStream();
			return false;
		case rmHeaders:
			if (readHeaderLine(tohKeyValue) && com.dataReady()) return checkStream();
			return false;
		case rmChunkHeader:
			if (readHeaderLine(tohChunkSize) && com.dataReady()) return checkStream();
			return false;
		case rmDirectUnlimited:
		case rmReadingChunk:
		case rmDirectLimited:
			if (skippingBody) {
				runSkipBody();
				if (rMode != rmEof) return false;
			}
			return true;
		case rmEof: return true;
		}
}

bool HttpResponse::processSingleHeader(TypeOfHeader toh, natural size) {
	char *buff = (char *)alloca(size);
	com.read(buff,size);
	ConstStrA buffer(buff,size-2);
	switch (toh) {
		case tohKeyValue: {
				if (buffer.empty()) {
					processHeaders();
					return true;
				}

				natural colon = buffer.find(':');
				if (colon == naturalNull)
					return invalidResponse(toh);
				ConstStrA key = buffer.head(colon);
				ConstStrA value = buffer.offset(colon+1);
				cropWhite(key);
				cropWhite(value);
				if (key == getHeaderFieldName(fldContentLength)) {
					TextParser<char, SmallAlloc<256> > parser;
					if (parser("%u1",value)) {
						hdrContentLength = parser[1];
					}
				}
				if (key == getHeaderFieldName(fldTransferEncoding)) {
					hdrTEChunked = value == "chunked";
				}
				if (key == getHeaderFieldName(fldConnection)) {
					StrCmpCI<char> cmp;
					if (cmp(value,"close") == cmpResultEqual) hdrConnection = hdrConnClose;
					else if (cmp(value,"keep-alive") == cmpResultEqual) hdrConnection = hdrConnKeepAlive;
					else if (cmp(value,"upgrade") == cmpResultEqual) hdrConnection = hdrConnUpgrade;
				}
				hdrCallback.storeHeaderLine(key,value);
			} break;
		case tohStatusLine: {
				//empty line in status is allowed here
				if (buffer.empty()) return false;
				TextParser<char, SmallAlloc<1500> > parser;
				if (parser("HTTP/1.%u1 %u2 %3",buffer)) {
					http11 = (natural)parser[1] == 1;
					status = parser[2];
					ConstStrA msg = parser[3].str();
					hdrCallback.storeStatus(status,msg);
					rMode = rmHeaders;
				} else {
					return invalidResponse(toh);
				}
			} break;
		case tohChunkSize: {
				if (buffer.empty()) break;
				TextParser<char, SmallAlloc<50> > parser;
				if (parser(" %[0-9a-fA-F]1 ",buffer)) {

					remainLength = parser[1].hex();
					rMode = remainLength?rmReadingChunk:rmEof;
					return true;

				} else {
					return invalidResponse(toh);
				}
			} break;
	}
	return false;

}
bool HttpResponse::readHeaderLine(TypeOfHeader toh) {
		bool rep;
		bool res;
		do {
			natural pos = ibuff.lookup(ConstBin(reinterpret_cast<const byte *>("\r\n"),2));
			if (pos == naturalNull) {
				natural f = ibuff.fetch();
				if (f == 0) {
					if (!com.canRead()) return endOfStream();
					else return invalidResponse(toh);
				}
				pos = ibuff.lookup(ConstBin(reinterpret_cast<const byte *>("\r\n"),2),f);
			}
			if (pos == naturalNull) {
				return false;
			}
			res = processSingleHeader(toh, pos+2);
			if (res == false && ibuff.getInputLength() != 0) {
				if (rMode == rmHeaders) {
					toh = tohKeyValue;
				}
				rep = true;
			} else {
				rep = false;
			}
		} while (rep);
		return res;




}

bool HttpResponse::endOfStream() {
	rMode = rmEof;
	return true;
}

bool HttpResponse::processHeaders() {

	if (status == 100) {
		rMode = rmContinue;
		keepAlive = true;
		return true;
	}

	TextParser<char, SmallAlloc<256> > parser;

	rMode = rmDirectUnlimited;

	if (hdrContentLength != naturalNull) {
			remainLength = hdrContentLength;
	}


	if (hdrTEChunked) {
		rMode = rmChunkHeader ;
		remainLength = 0;

	}


	if (rMode == rmDirectUnlimited && (status == 204 || status == 304 || status == 101)) {
		rMode = rmEof;
	} else if (rMode == rmDirectLimited && remainLength == 0) {
		rMode = rmEof;
	}

	skippingBody = false;

	switch (hdrConnection) {
		case hdrConnClose: keepAlive = false;break;
		case hdrConnKeepAlive: keepAlive = true;break;
		case hdrConnUpgrade: keepAlive = true;break;
		default: keepAlive = http11;
	}

	return true;
}

natural HttpResponse::read(void* buffer, natural size) {
	natural x;
	switch (rMode) {
	case rmHeaders:
	case rmChunkHeader:
	case rmStatus: readHeaders();return read(buffer,size);
	case rmDirectUnlimited:
		x = com.read(buffer,size);
		if (x == 0) rMode = rmEof;
		return x;
	case rmDirectLimited:
		if (size > remainLength) size = remainLength;
		x = com.read(buffer,size);
		if (x == 0) throw IncompleteStream(THISLOCATION);
		remainLength -= x;
		if (remainLength == 0) {
			rMode = rmEof;
		}
		return x;
	case rmReadingChunk:
		if (size > remainLength) size = remainLength;
		x = com.read(buffer,size);
		if (x == 0) throw IncompleteStream(THISLOCATION);
		remainLength -= x;
		if (remainLength == 0) {
			rMode = rmChunkHeader ;
		}
		return x;
	case rmEof:
	case rmContinue:
		return 0;
	}
}


natural HttpResponse::peek(void* buffer, natural size) const {
	natural x;
	switch (rMode) {
	case rmHeaders:
	case rmChunkHeader:
	case rmStatus: const_cast<HttpResponse *>(this)->readHeaders();return peek(buffer,size);
	case rmDirectUnlimited:
		return com.peek(buffer,size);break;
	case rmDirectLimited:
		if (size > remainLength) size = remainLength;
		x = com.peek(buffer,size);
		if (x == 0) throw IncompleteStream(THISLOCATION);
		return x;
	case rmReadingChunk:
		if (size > remainLength) size = remainLength;
		x = com.read(buffer,size);
		if (x == 0) throw IncompleteStream(THISLOCATION);
		return x;
	case rmEof:
	case rmContinue:
		return 0;
	}
}

bool HttpResponse::canRead() const {
	if (rMode == rmChunkHeader || rMode == rmHeaders || rMode == rmStatus) {
		const_cast<HttpResponse *>(this)->readHeaders();
	}
	return !(rMode == rmEof || rMode == rmContinue);
}

natural HttpResponse::dataReady() const {
	switch (rMode) {
		case rmContinue:
			return 1;
		case rmStatus:
		case rmHeaders:
		case rmChunkHeader:
			return com.dataReady()>0?1:0;
		case rmDirectUnlimited:
			return com.dataReady();
		case rmReadingChunk:
		case rmDirectLimited:
			return std::min(com.dataReady(), remainLength);
		case rmEof:
			return 1;
	}
}


bool HttpResponse::invalidResponse(TypeOfHeader toh) {
	throw MalformedResponse(THISLOCATION, toh);
}

static NamedEnumDef<HttpResponse::TypeOfHeader> tohStrDef[] = {
		{HttpResponse::tohChunkSize,"chunk"},
		{HttpResponse::tohKeyValue,"header"},
		{HttpResponse::tohStatusLine,"status"}
};

static NamedEnum<HttpResponse::TypeOfHeader> tohStr(tohStrDef);

void HttpResponse::MalformedResponse::message(ExceptionMsg &msg) const {
	msg("Malformed HTTP response in section: %1") << tohStr[section];
}

void HttpResponse::IncompleteStream::message(ExceptionMsg& msg) const {
	msg("Incomplete stream (unexpected end of stream)");
}

void HttpResponse::skipRemainBody(bool async)  {
	skippingBody = true;
	if (!async) {
		while (checkStream() == false);
	}
}

void HttpResponse::runSkipBody()  {
	byte buff[256];
	read(buff,256);
}

} /* namespace BredyHttpClient */

