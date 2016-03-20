/*
 * HttpStream.cpp
 *
 *  Created on: Jan 20, 2016
 *      Author: ondra
 */

#include "httpStream.h"

#include <lightspeed/base/namedEnum.tcc>
#include <lightspeed/base/text/textstream.tcc>
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
	return !outputClosed && remainLength > 0;
}

void HttpRequest::flush() {
	if (!inBody) return;
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
	if (chunk.length()) return;
	writeChunk(chunk);
	chunk.clear();
}

void HttpRequest::writeChunk(ConstBin data) {
	print("%1\n") << setBase(16) << data.length();
	com.writeAll(data.data(),data.length());
	print("\r\n");
}


HttpResponse::HttpResponse(IInputStream* com):com(*com),rMode(rmStatus) {
	if (com == 0) throwNullPointerException(THISLOCATION);

}

HttpResponse::HttpResponse(IInputStream* com, ReadHeaders):com(*com),rMode(rmStatus) {
	if (com == 0) throwNullPointerException(THISLOCATION);
	readHeaders();
}

HttpResponse::~HttpResponse() {
}

void HttpResponse::readHeaders() {
	natural p = checkStream();
	while (p == 0) {
		com.canRead(); //block if data are not yet available
		p = checkStream();
	}
}

natural HttpResponse::getStatus() const {
	return status;
}

ConstStrA HttpResponse::getStatusMessage() const {
	return statusMessage;
}

HttpResponse::HeaderValue HttpResponse::getHeaderField(Field field) const {
	return getHeaderField(getHeaderFieldName(field));
}

HttpResponse::HeaderValue HttpResponse::getHeaderField(ConstStrA field) const {
	const Str *r = headers.find(field);
	if (r == 0) return HeaderValue();
	else return HeaderValue(*r);
}

bool HttpResponse::isKeepAliveEnabled() const {
	return keepAlive;
}

natural HttpResponse::getContentLength() const {
	return remainLength;
}



natural HttpResponse::checkStream() {
	switch (rMode) {
		case rmContinue:
			if (com.dataReady() == 0) return 1;
			else {readHeaderLine(tohStatusLine);return 0;}
		case rmStatus:
			readHeaderLine(tohStatusLine);
			return 0;
		case rmHeaders:
			if (readHeaderLine(tohKeyValue)) return com.dataReady();
			else return 0;
		case rmChunkHeader:
			if (readHeaderLine(tohChunkSize)) return com.dataReady();
			else return 0;
		case rmDirectUnlimited:
			return com.dataReady();
		case rmReadingChunk:
		case rmDirectLimited: {
			natural k = com.dataReady();
			if (k > remainLength) k = remainLength;
			return k;
			}
		case rmEof: return 1;
		}
}

bool HttpResponse::readHeaderLine(TypeOfHeader toh) {
	char buff[15];
	natural cnt = com.peek(buff,sizeof(buff));
	ConstStrA strbuff(buff,cnt);
	natural pos = strbuff.find('\n');
	bool cancontinue = false;
	if (pos != naturalNull) {
		pos++;
		cancontinue = cnt > pos;
		cnt = pos;
	}
	buffer.append(strbuff.head(cnt));
	if (com.read(buff, cnt) == 0) {
		return endOfStream();
	}
	if (buffer.tail(2) == ConstStrA("\r\n")) {
		buffer.resize(buffer.length()-2);
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
				headers.replace(pool.add(key),pool.add(value));
				buffer.clear();
			} break;
		case tohStatusLine: {
				TextParser<char, SmallAlloc<1500> > parser;
				if (parser("HTTP/1.%u1 %u2 %3",buffer)) {
					http11 = (natural)parser[1] == 1;
					status = parser[2];
					ConstStrA msg = parser[3].str();
					statusMessage = pool.add(msg);
					rMode = rmHeaders;
					buffer.clear();
					toh = tohKeyValue;
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
					buffer.clear();
					return true;

				} else {
					return invalidResponse(toh);
				}
			} break;
		}
	}
	if (cancontinue)
		return readHeaderLine(toh);
	else
		return false;

}

bool HttpResponse::endOfStream() {
	rMode = rmEof;
	return true;
}

bool HttpResponse::processHeaders() {

	if (status == 100) {
		rMode = rmContinue;
		keepAlive = true;
	}

	TextParser<char, SmallAlloc<256> > parser;
	HeaderValue length = getHeaderField(fldContentLength);

	rMode = rmDirectUnlimited;

	if (length.defined) {
		if (parser("%u1",length)) {
			remainLength = parser[1];
			rMode = rmDirectLimited;
		} else {
			throw MalformedResponse(THISLOCATION, tohKeyValue);
		}
	}


	HeaderValue te = getHeaderField(fldTransferEncoding);
	if (te.defined) {
		if (te == ConstStrA("chunked")) {
			rMode = rmChunkHeader ;
			remainLength = 0;
		}
	}


	if (rMode == rmDirectUnlimited && (status == 204 || status == 304 || status == 101)) {
		rMode = rmEof;
	} else if (rMode == rmDirectLimited && remainLength == 0) {
		rMode = rmEof;
	}

	keepAlive = http11;
	HeaderValue conn = getHeaderField(fldConnection);
	if (conn.defined) {
		StrCmpCI<char> cmp;
		if (cmp(conn,"close") == cmpResultEqual) keepAlive = false;
		else if (cmp(conn,"keep-alive") == cmpResultEqual)  keepAlive = true;
		else if (cmp(conn,"upgrade") == cmpResultEqual) keepAlive = true;
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
		return com.read(buffer,size);break;
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
			if (com.dataReady() > 0) checkStream();
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
	return !(rMode == rmEof || rMode == rmContinue);
}

natural HttpResponse::dataReady() const {
	return const_cast<HttpResponse *>(this)->checkStream();
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

void HttpResponse::skipRemainBody()  {
	byte buff[256];
	while (read(buff,256));
}

} /* namespace BredyHttpClient */

