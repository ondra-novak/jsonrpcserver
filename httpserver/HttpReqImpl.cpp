/*
 * HttpReqImpl.cpp
 *
 *  Created on: Sep 7, 2012
 *      Author: ondra
 */

#include "HttpReqImpl.h"
#include "lightspeed/base/countof.h"
#include <lightspeed/base/containers/autoArray.tcc>
#include "lightspeed/base/text/textstream.tcc"
#include "ConnHandler.h"
#include <string.h>
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/mt/semaphore.h"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/exceptions/errorMessageException.h"
#include "lightspeed/base/exceptions/outofmemory.h"

namespace BredyHttpSrv {


static ConstStrA hdrfields[] = {
		/*fldHost ,*/ "Host",
		/*fldUserAgent,*/ "User-Agent",
		/*fldServer,*/ "Server",
		/*fldContentType,*/ "Content-Type",
		/*fldContentLength,*/ "Content-Length",
		/*fldConnection,*/ "Connection",
		/*fldCookie,*/ "Cookie",
		/*fldAccept, */"Accept",
		/*fldCacheControl,*/"Cache-Control",
		/*fldDate,*/ "Date",
		/*fldReferer,*/ "Referer",
		/*fldAllow,*/ "Allow",
		/*fldContentDisposition,*/ "Content-Disposition",
		/*fldExpires,*/ "Expires",
		/*fldLastModified,*/ "Last-Modified",
		/*fldLocation,*/ "Location",
		/*fldPragma,*/ "Pragma",
		/*fldRefresh,*/ "Referer",
		/*fldSetCookie,*/ "Set-Cookie",
		/*fldWWWAuthenticate,*/ "WWW-Authenticate",
		/*fldAuthorization,*/ "Authorization",
		/*fldWarning,*/ "Warning",
		/*fldAccessControlAllowOrigin,*/ "Access-Control-Allow-Origin",
		/*fldETag,*/ "ETag",
		/*fldIfNoneMatch,*/ "If-None-Match",
		/*fldIfModifiedSince,*/ "If-Modified-Since",
		/*fldTransferEncoding,*/ "Transfer-Encoding",
		/*fldExpect,*/ "Expect",
		/*fldUnknown,*/ "Undefined",
		/*fldUpgrade,*/ "Upgrade"
};

static ConstStrA statusMessages[] = {
		"100 Continue",
		"101 Switching Protocols",
		"200 OK",
		"201 Created",
		"202 Accepted",
		"203 Non-Authoritative Information",
		"204 No Content",
		"205 Reset Content",
		"206 Partial Content",
		"300 Multiple Choices",
		"301 Moved Permanently",
		"302 Found",
		"303 See Other",
		"304 Not Modified",
		"305 Use Proxy",
		"307 Temporary Redirect",
		"400 Bad Request",
		"401 Unauthorized",
		"402 Payment Required",
		"403 Forbidden",
		"404 Not Found",
		"405 Method Not Allowed",
		"406 Not Acceptable",
		"407 Proxy Authentication Required",
		"408 Request Timeout",
		"409 Conflict",
		"410 Gone",
		"411 Length Required",
		"412 Precondition Failed",
		"413 Request Entity Too Large",
		"414 Request-URI Too Long",
		"415 Unsupported Media Type",
		"416 Requested Range Not Satisfiable",
		"417 Expectation Failed",
		"426 Upgrade Required",
		"500 Internal Server Error",
		"501 Not Implemented",
		"502 Bad Gateway",
		"503 Service Unavailable",
		"504 Gateway Timeout",
		"505 HTTP Version Not Supported"
};

static ConstStrA fieldToText(IHttpRequest::HeaderField x) {
	natural idx = x;
	if (idx >= countof(hdrfields))
		return ConstStrA();

	return hdrfields[idx];
}
static ConstStrA getStatusMessage(natural status) {
	char buff[3];
	if (status >= 100 && status <= 999) {
		buff[0] = status / 100 + '0';
		buff[1] = (status / 10) % 10 + '0';
		buff[2] = status % 10 + '0';
		ConstStrA srchText(buff, 3);
		natural l = 0, h = countof(statusMessages);
		while (l < h) {
			natural m = (l + h) / 2;
			ConstStrA msg = statusMessages[m];
			ConstStrA code = msg.head(3);
			if (code > srchText)
				h = m;
			else if (code < srchText)
				l = m + 1;
			else
				return msg.offset(4);
		}
	}

	return ConstStrA("Unknown status code");
}

HttpReqImpl::HttpReqImpl(ConstStrA baseUrl, ConstStrA serverIdent, Semaphore &busySemaphore)
: inout(0)
, serverIdent(serverIdent)
, baseUrl(baseUrl)
, httpMajVer(1)
, httpMinVer(0)
, bHeaderSent(false)
, useChunked(false)
,statusCode(200)
,busySemaphore(busySemaphore)
,busyLockStatus(1)
,postBodyLimit(naturalNull)
	{

}

ConstStrA HttpReqImpl::getMethod() {
	return method;
}

ConstStrA HttpReqImpl::getPath() {
	return path;
}

ConstStrA HttpReqImpl::getProtocol() {
	return protocol;
}

const ConstStrA* HttpReqImpl::getHeaderField(ConstStrA field) const {
	HttpHeader::ListIter itr = inHeader.find(field);
	if (itr.hasItems()) return &itr.getNext();
	else return 0;
}

const ConstStrA* HttpReqImpl::getHeaderField(ConstStrA field, natural index) const {
	HttpHeader::ListIter itr = inHeader.find(field);
	while (index && itr.hasItems()) {
		index--;
		itr.skip();
	}
	if (itr.hasItems()) return &itr.getNext();
	else return 0;
}

bool HttpReqImpl::enumHeader(Message<bool, HeaderFieldPair> caller) const {
	for (HttpHeader::Iterator iter = inHeader.getFwIter(); iter.hasItems();) {
		const HttpHeader::Entity& e = iter.getNext();
		if (caller(HeaderFieldPair(e.key, e.value)))
			return true;
	}
	return false;
}

void HttpReqImpl::sendHeaders() {
	if (bHeaderSent)
		return;

	bNeedContinue = false;

	bool hasServer = false;
	bool hasContentType = false;
	bool hasTransfEnc = false;
	bool hasConnection = false;
	ConstStrA contentTypeKey = fieldToText(fldContentType);
	ConstStrA serverKey = fieldToText(fldServer);
	ConstStrA transfEnc = fieldToText(fldTransferEncoding);
	ConstStrA connectionStr = fieldToText(fldConnection);
	ConstStrA statusMsg = *(this->statusMsg);
	if (!this->statusMsg.isSet())
		statusMsg = "OK";

	SectionIODirect _(*this);

	PrintTextA print(*inout);
	print.setNL("\r\n");
	print("HTTP/%1.%2 %3 %4\n") << httpMajVer << httpMinVer << statusCode
			<< statusMsg;

	if (statusCode == 101) {
		hasTransfEnc = hasConnection = hasContentType = true;
		useChunked = false;
	}

	for (OutHdr::Iterator iter = outHdr.getFwIter(); iter.hasItems();) {
		const OutHdrPair& hdrPair = iter.getNext();
		if (!hasContentType && hdrPair.first == contentTypeKey)
			hasContentType = true;

		if (!hasServer && hdrPair.first == serverKey)
			hasServer = true;

		if (!hasTransfEnc && hdrPair.first == transfEnc)
			hasTransfEnc = !(useChunked && hdrPair.second == ConstStrA("chunked"));

		if (!hasConnection && hdrPair.first == connectionStr)
			hasConnection = true;


		print("%1: %2\n") << hdrPair.first << hdrPair.second;
	}
	if (!hasContentType)
		print("%1: %2\n") << contentTypeKey << "text/html;charset=UTF-8";

	if (!hasServer)
		print("%1: %2\n") << serverKey << serverIdent;

	if (!hasTransfEnc && useChunked)
		print("%1: %2\n") << transfEnc << "chunked";
	else
		useChunked = false;

	if (!hasConnection && closeConn)
		print("%1: %2\n") << connectionStr << "close";

	print("\n");
	outBuff.clear();
	//for code 100 or 101, additional header will be next
	if (statusCode == 100) {
		//clear current header
		outHdr.clear();
		//set status code 200 to simply processing reply (handler don't need to reset 100 status
		statusCode = 200;
		//unset message
		this->statusMsg.unset();
		//now, handler can exit function with status 100 - when data arrives, onData will be called
	} else {
		//header sent, prevent sending new headers
		bHeaderSent = true;
	}
	LogObject(THISLOCATION).info("HTTP/%1.%2 %3 %4 %5 %6")
			<< httpMajVer << httpMajVer << method << path << statusCode << statusMsg;
}
void HttpReqImpl::header(ConstStrA field, ConstStrA value) {
	if (bHeaderSent)
		throw ErrorMessageException(THISLOCATION, "Headers already sent");

	outHdr.add(OutHdrPair(addString(field), addString(value)));
}

void HttpReqImpl::status(natural code, ConstStrA msg) {
	if (bHeaderSent)
		throw ErrorMessageException(THISLOCATION, "Headers already sent");

	statusCode = code;
	if (msg.empty())
		msg = getStatusMessage(statusCode);

	statusMsg = addString(msg);
}
void HttpReqImpl::errorPage(natural code, ConstStrA msg, ConstStrA expl) {
	if (msg.empty()) msg = getStatusMessage(code);
	SeqFileOutput f(this);
	if (msg.empty()) msg = getStatusMessage(code);
	PrintTextA print(f);
	if (!bHeaderSent) {
		status(code,msg);
	}
	print("<html><head><title>%1 %2</title></head><body><h1>%1 %2</h1>")
		<< code << msg;
	if (!expl.empty()) print("<pre>%3</pre>")<< expl;
	print("</body></html>");
}

ConstStrA HttpReqImpl::getBaseUrl() const {
	return baseUrl;
}


void HttpReqImpl::redirect(ConstStrA url, int code) {
	if (code == 0)
		code = 302;

	if (url.head(7) != ConstStrA())
	header(fldLocation, url);
	errorPage(code,ConstStrA(),url);
}
void HttpReqImpl::useHTTP11(bool use) {
	httpMinVer = use ? 1 : 0;
	useChunked = use;
	if (use == false) closeConn = true;
	else {
		const ConstStrA *h = getHeaderField(fldConnection);
		if (h && *h == "close") closeConn = true;
		else closeConn = false;
	}
}

void HttpReqImpl::openPostChunk() const {
	SectionIODirect _(*this);
	ScanTextA scn(*inout);
	scn.setNL("\r\n");
	scn("\n%");
	if (scn(" %(+)[0-9A-Za-z]1 \n%")) {
		remainPostData = scn[1].hex();
		if (remainPostData == 0) {
			chunkedPost = false;
			if (scn("\n%") == false)
				throw HttpStatusException(THISLOCATION,path,400,"Bad request");
		} if (postBodyLimit < remainPostData) {
			throw HttpStatusException(THISLOCATION, path, 413, "Request is too large");
		} else {
			postBodyLimit-= remainPostData;
		}
	} else {
		throw HttpStatusException(THISLOCATION,path,400,"Bad request");
	}
}

natural HttpReqImpl::read(void* buffer, natural size) {
	if (bNeedContinue) send100continue();
	SectionIODirect _(*this);
	if (remainPostData == 0) {
		if (chunkedPost) openPostChunk();
		if (remainPostData == 0) return 0;
	}
	if (size > remainPostData) size = remainPostData;
	natural rd = inout->blockRead(buffer,size,false);
	remainPostData -= rd;
	return rd;
}

void HttpReqImpl::writeChunk(const void* data, natural len) {
	if (len == 0)
		return;

	SectionIODirect _(*this);

	PrintTextA p(*inout);
	p.setNL("\r\n");
	p.setBase(16);
	p("%1\n") << len;
	inout->blockWrite(data, len, true);
	p("\n");
}
natural HttpReqImpl::write(const void* buffer, natural size) {
	if (!bHeaderSent) {
		sendHeaders();
		if (!bHeaderSent) {
			throw ErrorMessageException(THISLOCATION, "Invalid write to stream, when status 1XX is used. New header must be prepared");
		}
	}

	if (isHeadMethod)
		return size;

	if (useChunked) {
		if (size < outBuff.getAllocatedSize() - outBuff.length()) {
			outBuff.append(ConstStrA((const char*) (buffer), size));
		} else if (size < outBuff.getAllocatedSize()) {
			writeChunk(outBuff.data(), outBuff.length());
			outBuff.clear();
			outBuff.append(ConstStrA((const char*) (buffer),size));
		} else {
			writeChunk(outBuff.data(),outBuff.length());
			outBuff.clear();
			writeChunk(buffer,size);
		}
		return size;
	} else {
		SectionIODirect _(*this);
		return inout->blockWrite(buffer, size, false);
	}
}
natural HttpReqImpl::peek(void* buffer, natural size) const {
	SectionIODirect _(*this);
	if (bNeedContinue) send100continue();
	if (remainPostData == 0) {
		if (chunkedPost) openPostChunk();
		if (remainPostData == 0) return 0;
	}
	char *b = reinterpret_cast<char *>(buffer);
	*b = inout->peek();
	return 1;
}

bool HttpReqImpl::canRead() const {
	if (remainPostData == 0) {
		if (chunkedPost) openPostChunk();
	}

	return remainPostData > 0;
}

bool HttpReqImpl::canWrite() const {
	return !outputClosed;
}

void HttpReqImpl::flush() {
	inout->flush();
}

const ConstStrA* HttpReqImpl::getHeaderField(HeaderField field) const {
	return getHeaderField(fieldToText(field));
}

const ConstStrA* HttpReqImpl::getHeaderField(HeaderField field, natural index) const {
	return getHeaderField(fieldToText(field), index);
}

void HttpReqImpl::header(HeaderField field, ConstStrA value) {
	header(fieldToText(field), value);
}

bool HttpReqImpl::isField(ConstStrA text, HeaderField fld) const {
	return text == fieldToText(fld);
}

natural HttpReqImpl::dataReady() const {
	if(canRead() ) return inout->dataReady(); else return 0;
}

natural HttpReqImpl::callHandler(ConstStrA path, IHttpHandler **h) {
	return callHandler(*this,path,h);
}

natural HttpReqImpl::callHandler(IHttpRequest& request, IHttpHandler **h) {
	return callHandler(request,request.getPath(),h);
}

natural HttpReqImpl::forwardRequest(ConstStrA path) {
	IHttpHandler *h;
	natural r = callHandler(*this,path,&h);
	if (h != 0) curHandler = h;
	return r;

}


void HttpReqImpl::finishChunk() {
	while (remainPostData > 0) {
		char buff[256];
		read(buff,256);
	}
	if (useChunked) {
		if (!outBuff.empty()) {
			writeChunk(outBuff.data(),outBuff.length());
			outBuff.clear();
		}
		PrintTextA p(*inout);
		p.setNL("\r\n");
		p("0\n\n");
	}
	flush();
}
HttpReqImpl::OutHdrBuffPart HttpReqImpl::addString(ConstStrA str) {
	natural p = outBuff.length();
	outBuff.append(str);
	return OutHdrBuffPart(outBuff, p, str.length());
}


bool HttpReqImpl::HttpHeaderCmpKeys::operator ()(ConstStrA a, ConstStrA b) const {

	natural minl = std::min(a.length(),b.length());
	int cmp = strncasecmp(a.data(),b.data(),minl);
	if (cmp == 0) return a.length() < b.length();
	else return cmp < 0;
}


bool HttpReqImpl::onData(NStream& stream) {
	inout = &stream;
	try {
		if (curHandler != nil) {
			SectionIODirectRev _(*this);
			natural res = curHandler->onData(*this);
			return processHandlerResponse(res);
		} else {
			return readHeader();
		}
	} catch (HttpStatusException &e) {
		LogObject lg(THISLOCATION);
		lg.note("Status set by exception: %1") << e.getMessage();
		useHTTP11(false);
		errorPage(e.getStatus(),e.getStatusMsg().getUtf8(),ConstStrA());
		return ITCPServerConnHandler::cmdRemove;
	} catch (LightSpeed::NotImplementedExcption &e) {
		LogObject lg(THISLOCATION);
		lg.note("Http unhandled exception: %1") << e.getMessage();
		useHTTP11(false);
		errorPage(501,ConstStrA(),e.getMessage().getUtf8());
		return ITCPServerConnHandler::cmdRemove;

	} catch (std::exception &e) {
		LogObject lg(THISLOCATION);
		lg.note("Http unhandled exception: %1") << e.what();
		useHTTP11(false);
		errorPage(500,ConstStrA(),e.what());
		return ITCPServerConnHandler::cmdRemove;
	} catch (...) {
		LogObject lg(THISLOCATION);
		lg.note("Http unhandled exception: ...") ;
		useHTTP11(false);
		errorPage(500,ConstStrA());
		return ITCPServerConnHandler::cmdRemove;
	}
}



bool HttpReqImpl::readHeader() {

	try {
		inout->peek(); //force fill buffer with data from input stream
		while (inout->dataReady()) {
			char s = (char)inout->getNext();
			headerData.add(s);
			if (s == '\n' && headerData.tail(4) == ConstStrA("\r\n\r\n")) {
				SectionIODirectRev _(*this);
				headerData.erase(headerData.length(),2);
				return finishReadHeader();
			}
		}
		return true;
	} catch (AllocatorLimitException &e) {
		errorPage(413,ConstStrA(),"Header is too large (4096 bytes)");
		return false;
	}
}

static void crop(ConstStrA &k) {
	while (!k.empty()) {
		if (isspace(k[0])) k = k.crop(1,0);
		else if (isspace(k[k.length()-1])) k = k.crop(0,1);
		else break;
	}
}

bool HttpReqImpl::finishReadHeader() {
	HeaderData::SplitIterator iter = headerData.split('\n');
	ConstStrA firstLine = iter.getNext();
	firstLine = firstLine.crop(0,1);
	ConstStrA::SplitIterator fsplt = firstLine.split(' ');
	method = fsplt.getNext();
	if (!fsplt.hasItems()) {
		return errorPageKA(400,"First line is strange (after method)");
	}
	path = fsplt.getNext();
	if (!fsplt.hasItems()) {
		return errorPageKA(400,"First line is strange (after path)");
	}
	protocol = fsplt.getNext();
	if (fsplt.hasItems()) {
		return errorPageKA(400,"First line is strange (after proto)");
	}
	if (protocol != ConstStrA("HTTP/1.0") && protocol != ConstStrA("HTTP/1.1")) {
		return errorPageKA(400,StringA(ConstStrA("Unknown protocol: ")+protocol));
	}
	while (iter.hasItems()) {
		ConstStrA itm = iter.getNext();
		if (!itm.empty()) {
			itm = itm.crop(0,1);
			if (!itm.empty()) {
				natural p = itm.find(':');
				if (p != naturalNull) {
					ConstStrA key = itm.head(p);
					ConstStrA value = itm.offset(p+1);
					crop(key);
					crop(value);
					if (key.empty()) {
						return errorPageKA(400,"Empty key in header");
					} else {
						inHeader.insert(key,value);
					}
				} else {
					return errorPageKA(400,"Strange line in the header");
				}
			}
		}
	}
	bufferAllocPos = headerData.getAllocatedSize();

	//parse some fields
	TextParser<char,StaticAlloc<256> > parser;
	isHeadMethod = method == "HEAD";
	closeConn = true;

	//check http version
	if (parser("HTTP/%u1.%u2",protocol)) {
			httpMajVer = (unsigned short)((natural)parser[1]);
			httpMinVer = (unsigned short)((natural)parser[2]);
			if (httpMajVer != 1 || (httpMinVer != 0 && httpMinVer != 1))
				return errorPageKA(505);
			useHTTP11(httpMinVer == 1);
	} else {
		return errorPageKA(400,StringA(ConstStrA("Unknown protocol: ")+ protocol));
	}

	//check content length
	const ConstStrA *strsize = getHeaderField(fldContentLength);
	if (strsize != 0 && parser(" %u1 ", *strsize)) {
		remainPostData = parser[1];
	} else {
		remainPostData = 0;
	}

	//check chunked POST
	const ConstStrA *strchunked = getHeaderField(fldTransferEncoding);
	if (strchunked != 0 && *strchunked == "chunked") {
		chunkedPost = true;
		remainPostData = 0; //remainPostData is used to count chunks
		openPostChunk();
	} else {
		chunkedPost = false;
	}

	//check expectation for 100-continue
	const ConstStrA *strexpect = getHeaderField(fldExpect);
	if (strexpect != 0) {
		if (*strexpect == "100-continue" || *strexpect == "100-Continue") {
			bNeedContinue = true;
		} else {
			return errorPageKA(417);
		}
	} else {
		bNeedContinue = false;
	}

	//find handler
	IHttpHandler *h;
	curHandler = nil;
	outputClosed = false;
	natural res = callHandler(*this,path, &h);
	if (h == 0) return errorPageKA(404);
	if (curHandler == nil) curHandler = h; //need this to correctly handle forward function
	return processHandlerResponse(res);
}


bool HttpReqImpl::processHandlerResponse(natural res) {
	//depend on whether headers has been sent
	if (bHeaderSent) {
		//in case of 100 response, keep connectnion online
		if (res == 100) return true;
		else {
			if (!outputClosed) {
				//otherwise finalize chunk
				finishChunk();
			}
			//clear connection status
			clear();
			//keep conection depend on keep-alive
			return keepAlive();
		}
	} else {
		//in case of 100 response
		if (res == 100) {
			//whether need 100 Continue
			if (bNeedContinue) {
				//send this header
				send100continue();
			}
			//keep connection alive
			return true;
		}
		//otherwise response with status page
		return errorPageKA(res);
	}
}

void HttpReqImpl::clear() {
	inHeader.clear();
	headerData.clear();
	curHandler = nil;
	bHeaderSent = false;
	isHeadMethod = false;
	useChunked = false;
	inout = nil;
	outBuff.clear();
	outHdr.clear();
	statusMsg.unset();
	remainPostData = 0;
	chunkedPost = false;
	if ((char *)(connectionContext.get()) > headerData.data() &&
			(char *)(connectionContext.get()) < headerData.data() + headerData.length()) {
		connectionContext = nil;
	}
	requestContext = nil;
	statusCode=200;
	postBodyLimit = naturalNull;

}

void HttpReqImpl::finish() {
	finishChunk();
	clear();
}

bool HttpReqImpl::errorPageKA(natural code, ConstStrA expl) {
	errorPage(code,ConstStrA(),expl);
	finishChunk();
	clear();
	return !closeConn;
}


void *HttpReqImpl::alloc(natural objSize) {
	IRuntimeAlloc *owner;
	return alloc(objSize,owner);
}
void HttpReqImpl::dealloc(void *ptr, natural objSize) {
	char *p = (char *)ptr;
	if (p >= headerData.data() && p < headerData.data()+headerData.getAllocatedSize()) {
		if (p == headerData.data() + bufferAllocPos) bufferAllocPos+=objSize;
	} else {
		return StdAlloc::getInstance().dealloc(ptr,objSize);
	}
}
void *HttpReqImpl::alloc(natural objSize, IRuntimeAlloc * &owner) {
	if (objSize > (bufferAllocPos - headerData.length())) {
		return StdAlloc::getInstance().alloc(objSize,owner);
	} else {
		owner = this;
		bufferAllocPos -= objSize;
		return headerData.data()+bufferAllocPos;
	}
}
void HttpReqImpl::setRequestContext(IHttpHandlerContext *context) {
	requestContext = context;
}
void HttpReqImpl::setConnectionContext(IHttpHandlerContext *context) {
	connectionContext = context;
}
IHttpHandlerContext *HttpReqImpl::getRequestContext() const {
	return requestContext.get();
}
IHttpHandlerContext *HttpReqImpl::getConnectionContext() const {
	return connectionContext.get();
}
IRuntimeAlloc &HttpReqImpl::getContextAllocator() {
	return *this;
}


natural HttpReqImpl::getPostBodySize() const {
	if (chunkedPost) return canRead()?naturalNull:0;
	return remainPostData;
}

void HttpReqImpl::send100continue() const {
	PrintTextA print(*inout);
	print.setNL("\r\n");
	print("HTTP/1.1 100 Continue\n");
	print("\n");
	bNeedContinue = false;

}
/* namespace jsonsvc */

void HttpReqImpl::beginIO() {
	if (busyLockStatus == 0) {
		busySemaphore.unlock();
	}
	busyLockStatus++;
}
void HttpReqImpl::endIO() {
	if (busyLockStatus != 0) {
		busyLockStatus--;
		if (busyLockStatus == 0) {
			busySemaphore.lock();
		}
	}

}

PNetworkStream HttpReqImpl::getConnection() {
	inout->flush();
	return inout->getHandle().getMT();
}

void HttpReqImpl::setMaxPostSize(natural bytes) {
	if (remainPostData > bytes) throw HttpStatusException(THISLOCATION,path,413,"Request is too large");
	postBodyLimit = bytes;

}

void HttpReqImpl::closeOutput() {
	flush();
	finishChunk();
	outputClosed = true;
	if (!keepAlive()) {
		inout->flush();
		inout->getHandle()->closeOutput();
	}
}


}

