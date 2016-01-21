/*
 * HttpStream.cpp
 *
 *  Created on: Jan 20, 2016
 *      Author: ondra
 */

#include "httpStream.h"
#include <lightspeed/base/text/textstream.tcc>
namespace BredyHttpClient {

HttpRequest::HttpRequest(IOutputStream *com, ConstStrA path,ConstStrA method, bool useHTTP11)
:bodyHandling(useHTTP11?useChunked:useBuffered),inBody(false),print(SeqFileOutput(com)),com(*com)
,chunkMinSize(defaultChunkMinSize),chunkMaxSize(defaultChunkMaxSize),outputClosed(false)

{
	initRequest(path,method,useHTTP11);
}
HttpRequest::HttpRequest(IOutputStream *com, ConstStrA path,Method method, bool useHTTP11)
:bodyHandling(useHTTP11?useChunked:useBuffered),inBody(false),print(SeqFileOutput(com)),com(*com)
,chunkMinSize(defaultChunkMinSize),chunkMaxSize(defaultChunkMaxSize),outputClosed(false)

{
	initRequest(path,getMethodName(method),useHTTP11);
}


void HttpRequest::initRequest(ConstStrA path,ConstStrA method, bool useHTTP11) {
	print.setNL("\r\n");
	print("%1 %2 HTTP/1.%3") << method << path << (useHTTP11?1:0);
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
		bodyHandling == useDefinedByUser;
	}
	print("%1: %2\n") << hdrField << value;
}

void HttpRequest::beginBody() {
	if (inBody) throw ;//HeadersAlreadySentException(THISLOCATION);
	inBody = true;
	switch (bodyHandling) {
		//we don't care about length - set length to some number not equal to zero
	case useDefinedByUser: remainLength = 100; //NOT break here is WAI
		//terminate header block and open stream
	case useRemainLength: print("\n");break;
		//declare transfer encoding and finish header block
	case useChunked: print("%1: %2\n\n") << getHeaderFieldName(fldTransferEncoding) << "chunked";
		//do nothing, keep headers opened, it need to write Content-Length at the end
	case useBuffered: break;
	}
}


} /* namespace BredyHttpClient */
