/*
 * HttpReqImpl.cpp
 *
 *  Created on: Sep 7, 2012
 *      Author: ondra
 */

#include <string.h>
#include "HttpReqImpl.h"
#include "lightspeed/base/countof.h"
#include <lightspeed/base/containers/autoArray.tcc>
#include "lightspeed/base/text/textstream.tcc"
#include "ConnHandler.h"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/mt/semaphore.h"
#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/exceptions/errorMessageException.h"
#include "lightspeed/base/exceptions/outofmemory.h"
#include "lightspeed/base/containers/stringpool.tcc"
#include "lightspeed/base/containers/carray.h"
#include "lightspeed/base/interface.tcc"
#include "lightspeed/base/streams/fileiobuff.tcc"
#include "lightspeed/base/containers/map.tcc"

//
namespace BredyHttpSrv {


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

HttpReqImpl::HttpReqImpl(StringA serverIdent, const PBusyThreadsControl &busySemaphore)
:serverIdent(serverIdent)
,busySemaphore(busySemaphore)
,inout(0)
, httpMajVer(1)
, httpMinVer(0)
, bHeaderSent(false)
, useChunked(false)
,switchedProtocol(false)
,statusCode(200)
,postBodyLimit(naturalNull)
{

}

ConstStrA HttpReqImpl::getMethod() const {
	return method;
}

ConstStrA HttpReqImpl::getPath() const {
	return path;
}

ConstStrA HttpReqImpl::getProtocol() const {
	return protocol;
}

HeaderValue HttpReqImpl::getHeaderField(ConstStrA field) const {
	const HdrStr *val = requestHdrs.find(field);
	if (val == 0) return HeaderValue();
	else return HeaderValue(ConstStrA(*val));

}

bool HttpReqImpl::enumHeader(Message<bool, HeaderFieldPair> caller) const {
	for (HeaderMap::Iterator iter = requestHdrs.getFwIter(); iter.hasItems();) {
		const HeaderMap::Entity& e = iter.getNext();
		if (caller(HeaderFieldPair(e.key, e.value)))
			return true;
	}
	return false;
}


void HttpReqImpl::sendHeaders() {
	if (bHeaderSent)
		return;

	if (bNeedContinue) {
		remainPostData = 0;
		chunkedPost = false;
	}
	bNeedContinue = false;

	bool hasServer = false;
	bool hasContentType = false;
	bool hasTransfEnc = false;
	bool hasConnection = false;
	bool hasLength = false;
	bool hasDate = false;
	static ConstStrA contentTypeKey = getHeaderFieldName(fldContentType);
	static ConstStrA serverKey = getHeaderFieldName(fldServer);
	static ConstStrA transfEnc = getHeaderFieldName(fldTransferEncoding);
	static ConstStrA connectionStr = getHeaderFieldName(fldConnection);
	static ConstStrA contenLenStr = getHeaderFieldName(fldContentLength);
	static ConstStrA dateStr = getHeaderFieldName(fldDate);
	ConstStrA statusMsgStr = this->statusMsg;
	if (statusMsgStr.empty())
		statusMsgStr = getStatusMessage(statusCode);


	PrintTextA print(*inout);
	print.setNL("\r\n");
	print("HTTP/%1.%2 %3 %4\n") << httpMajVer << httpMinVer << statusCode
			<< statusMsgStr;

	if (statusCode == 101) {
		hasTransfEnc = hasConnection = hasContentType = true;
		useChunked = false;
		remainPostData = naturalNull;
		switchedProtocol = true;
		TimeStamp reqEndTime = TimeStamp::now();
		natural reqTime = (reqEndTime - reqBeginTime).getMilis();
		logRequest(reqTime);
	}

	for (HeaderMap::Iterator iter = responseHdrs.getFwIter(); iter.hasItems();) {
		const HeaderMap::Entity hdrPair = iter.getNext();
		if (!hasContentType && hdrPair.key == contentTypeKey)
			hasContentType = true;

		if (!hasServer && hdrPair.key == serverKey)
			hasServer = true;

		if (!hasTransfEnc && hdrPair.key == transfEnc)
			hasTransfEnc = !(useChunked && hdrPair.value == ConstStrA("chunked"));

		if (!hasConnection && hdrPair.key == connectionStr)
			hasConnection = true;

		if (!hasDate && hdrPair.key == dateStr)
			hasDate = true;

		if (!hasLength && hdrPair.key == contenLenStr) {
			hasLength = true;
		}

		print("%1: %2\n") << ConstStrA(hdrPair.key) << ConstStrA(hdrPair.value);
	}
	if (!hasContentType)
		print("%1: %2\n") << contentTypeKey << "text/html;charset=UTF-8";

	if (!hasServer)
		print("%1: %2\n") << serverKey << serverIdent;

	if (hasLength) {
		useChunked = false;
	}

	if (!hasDate) {
		TimeStamp::RFC1123Time datenow = TimeStamp::now().asRFC1123Time();
		print("%1: %2\n") << dateStr << ConstStrA(datenow);
	}

	if (!hasTransfEnc && useChunked && !closeConn)
		print("%1: %2\n") << transfEnc << "chunked";
	else
		useChunked = false;

	if (!hasConnection && closeConn)
		print("%1: %2\n") << connectionStr << "close";

	print("\n");

/*	LogObject(THISLOCATION).progress("%7 - %3 %4 HTTP/%1.%2 %5 %6")
		<< httpMajVer << httpMajVer << ConstStrA(method)
		<< ConstStrA(path) << statusCode << statusMsgStr
		<< getIfc<IHttpPeerInfo>().getPeerRealAddr();*/

	responseHdrs.clear();
	//for code 100 or 101, additional header will be next
	if (statusCode == 100) {
		//set status code 200 to simply processing reply (handler don't need to reset 100 status
		statusCode = 200;
		//unset message
		this->statusMsg = HdrStr();
		//now, handler can exit function with status 100 - when data arrives, onData will be called
	} else {
		//header sent, prevent sending new headers
		bHeaderSent = true;
	}
}
void HttpReqImpl::header(ConstStrA field, ConstStrA value) {
	if (bHeaderSent)
		throw ErrorMessageException(THISLOCATION, "Headers already sent");

	responseHdrs.insert(responseHdrPool.add(field),responseHdrPool.add(value));
}

void HttpReqImpl::status(natural code, ConstStrA msg) {
	if (bHeaderSent)
		throw ErrorMessageException(THISLOCATION, "Headers already sent");

	statusCode = code;
	if (msg.empty())
		msg = getStatusMessage(statusCode);

	statusMsg = responseHdrPool.add(msg);
}
void HttpReqImpl::errorPage(natural code, ConstStrA msg, ConstStrA expl) {
	if (!bHeaderSent) {
		if (msg.empty()) msg = getStatusMessage(code);
		SeqFileOutput f(this);
		if (msg.empty()) msg = getStatusMessage(code);
		PrintTextA print(f);
		//clear some headers - they can damage output
		//delete content length - because we will show different page
		responseHdrs.erase(getHeaderFieldName(fldContentLength));
		//delete transfer encoding - because we need simplest encoding
		responseHdrs.erase(getHeaderFieldName(fldTransferEncoding));
		//delete content type - because we will set to text/html
		responseHdrs.erase(getHeaderFieldName(fldContentType));
		//delete eTag - don't store eTag with an error page
		responseHdrs.erase(getHeaderFieldName(fldETag));
		//delete lastModified - don't store error page
		responseHdrs.erase(getHeaderFieldName(fldLastModified));
		status(code,msg);
		//set new content type
		header(fldContentType,"text/html");

		if (code == 204 || code == 304) {
			//set ContentLength to zero even if it is not necesery, some clients can have issue
			header(fldContentLength,"0");
			sendHeaders();
			return;
		} else {
			closeConn = true;
		}
		print("<html><head><title>%1 %2</title></head><body><h1>%1 %2</h1>")
				<< code << msg;
		if (!expl.empty()) print("<pre>%1</pre>")<< expl;
		print("<hr>");
		print("<small><em>Powered by Bredy's JsonRpcServer - C++ http & jsonrpc server - <a href=\"https://github.com/ondra-novak/jsonrpcserver\">sources available</a></em></small>");
		print("</body></html>");
	}
	else {
		closeConn = true;
	}
}



void HttpReqImpl::redirect(ConstStrA url, int code) {
	if (code == 0) {
		if (url.head(1) == ConstStrA('+')) code = 301;
		else code = 307;
	}


	TextParser<char> parser;
	if (parser("%[a-z]0://%",url)) {
		header(fldLocation, url);
	} else {
		StringA absurl = getAbsoluteUrl(url);
		header(fldLocation, absurl);
	}
	header(fldContentLength, "0");
	status(code);
	sendHeaders();

}
void HttpReqImpl::useHTTP11(bool use) {
	httpMinVer = use ? 1 : 0;
	useChunked = use;
	if (use == false) closeConn = true;
	else {
		HeaderValue h = getHeaderField(fldConnection);
		if (h.defined && h == "close") closeConn = true;
		else closeConn = false;
	}
}

void HttpReqImpl::openPostChunk() const {
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
	if (inputClosed) return 0;
	if (switchedProtocol) {
		natural rd = inout->blockRead(buffer,size);
		if (rd == 0) inputClosed = true;
		return rd;
	}
	if (remainPostData == 0) {
		if (chunkedPost) openPostChunk();
		if (remainPostData == 0) {
			inputClosed = true;
			return 0;
		}
	}
	if (size > remainPostData) size = remainPostData;
	natural rd = inout->blockRead(buffer,size,false);
	remainPostData -= rd;
	return rd;
}

void HttpReqImpl::writeChunk(const void* data, natural len) {
	if (len == 0)
		return;


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
		if (size <= chunkBuff.getAllocatedSize() - chunkBuff.length()) {
			chunkBuff.append(ConstBin(reinterpret_cast<const byte *>(buffer), size));
		} else if (size < chunkBuff.getAllocatedSize()) {
			chunkBuff.append(ConstBin(reinterpret_cast<const byte *>(buffer), size));
			writeChunk(chunkBuff.data(), chunkBuff.length());
			chunkBuff.clear();
		} else {
			writeChunk(chunkBuff.data(),chunkBuff.length());
			chunkBuff.clear();
			writeChunk(buffer,size);
		}
		return size;
	} else {
		return inout->blockWrite(buffer, size, false);

	}
}
natural HttpReqImpl::peek(void* buffer, natural size) const {
	if (bNeedContinue) send100continue();
	if (inputClosed || size == 0) return 0;
	if (remainPostData == 0) {
		if (chunkedPost) openPostChunk();
		if (remainPostData == 0) {
			inputClosed = true;
			return 0;
		}
	}
	char *b = reinterpret_cast<char *>(buffer);
	*b = inout->peek();
	return 1;
}

bool HttpReqImpl::canRead() const {
	if (inputClosed) return false;
	if (switchedProtocol) return true;
	if (remainPostData == 0) {
		if (chunkedPost) openPostChunk();
		if (remainPostData == 0) {
			inputClosed = true;
			return false;
		}

	}
	return true;
}

bool HttpReqImpl::canWrite() const {
	return !outputClosed;
}

void HttpReqImpl::flush() {
	inout->flush();
}

HeaderValue HttpReqImpl::getHeaderField(HeaderField field) const {
	return getHeaderField(getHeaderFieldName(field));
}


void HttpReqImpl::header(HeaderField field, ConstStrA value) {
	header(getHeaderFieldName(field), value);
}

bool HttpReqImpl::isField(ConstStrA text, HeaderField fld) const {
	return text == getHeaderFieldName(fld);
}

natural HttpReqImpl::dataReady() const {
	if(canRead() ) return inout->dataReady(); else return 0;
}



natural HttpReqImpl::forwardRequest(ConstStrA vpath, IHttpHandler **h) {
	IHttpHandler *hx;
	natural r = callHandler(vpath,&hx);
	if (hx != 0) curHandler = hx;
	if (h) *h = hx;
	return r;

}

natural HttpReqImpl::forwardRequestTo(IHttpHandler *h, ConstStrA vpath) {
	curHandler = h;
	return h->onRequest(*this,vpath);
}



void HttpReqImpl::finishChunk() {
	if (!bNeedContinue && !closeConn) {
		while (remainPostData > 0) {
			char buff[256];
			read(buff,256);
		}
	}
	if (useChunked) {
		if (!chunkBuff.empty()) {
			writeChunk(chunkBuff.data(),chunkBuff.length());
			chunkBuff.clear();
		}
		PrintTextA p(*inout);
		p.setNL("\r\n");
		p("0\n\n");
	}
	flush();
}

bool HttpReqImpl::HttpHeaderCmpKeys::operator ()(ConstStrA a, ConstStrA b) const {

	StrCmpCI<char> cmp;
	return cmp(a,b) == cmpResultLess;
}


ITCPServerConnHandler::Command HttpReqImpl::onData(NStream& stream, EventType eventType, natural reason) {


	inout = &stream;
	try {
		if (curHandler != nil) {
			evType = eventType;
			wakeUpReason = reason;
			natural res = curHandler->onData(*this);
			return processHandlerResponse(res);

		} else if (eventType == evDataReady) {
			//only evDataReady is expected
			evType = evRequest;
			return readHeader();
		} else {
			//otherwise it is invalid
			return ITCPServerConnHandler::cmdRemove;
		}
	} catch (HttpStatusException &e) {
		LogObject lg(THISLOCATION);
		lg.note("Status set by exception: %1") << e.getMessage();
		useHTTP11(false);
		if (inout != 0) errorPage(e.getStatus(),e.getStatusMsg().getUtf8(),ConstStrA());
		return ITCPServerConnHandler::cmdRemove;
	} catch (LightSpeed::NotImplementedExcption &e) {
		LogObject lg(THISLOCATION);
		lg.note("Http unhandled exception: %1") << e.getMessage();
		useHTTP11(false);
		if (inout != 0) errorPage(501,ConstStrA(),e.getMessage().getUtf8());
		return ITCPServerConnHandler::cmdRemove;

	} catch (std::exception &e) {
		LogObject lg(THISLOCATION);
		lg.note("Http unhandled exception: %1") << e.what();
		useHTTP11(false);
		if (inout != 0) errorPage(500,ConstStrA(),e.what());
		return ITCPServerConnHandler::cmdRemove;
	} catch (...) {
		LogObject lg(THISLOCATION);
		lg.note("Http unhandled exception: ...") ;
		useHTTP11(false);
		if (inout != 0) errorPage(500,ConstStrA());
		return ITCPServerConnHandler::cmdRemove;
	}
}



ITCPServerConnHandler::Command  HttpReqImpl::readHeader() {

	try {

		AutoArray<char, SmallAlloc<8192> > linebuff;

		NStream::Buffer &buffer = inout->getBuffer();
		natural fetched = buffer.fetch();
		if (fetched == 0) {
			errorPage(413,ConstStrA(),"Header is too large");
			return ITCPServerConnHandler::cmdRemove;
		}
		natural pos = buffer.lookup(ConstBin("\r\n"),fetched);
		while (pos != naturalNull) {

			linebuff.resize(pos+2);
			buffer.read(linebuff.data(),pos+2);
			ConstStrA line = linebuff.head(pos);

			if (method.empty()) {
				if (pos == 0) return ITCPServerConnHandler::cmdWaitRead;

				reqBeginTime = TimeStamp::now();
				reportDuration = true;

				cropWhite(line);
				ConstStrA::SplitIterator splt = line.split(' ');
				method = hdrPool.add(ConstStrA(splt.getNext()));
				path = hdrPool.add(ConstStrA(splt.getNext()));
				while (path.empty()) path = hdrPool.add(ConstStrA(splt.getNext()));
				protocol = hdrPool.add(ConstStrA(splt.getNext()));
				while (protocol.empty()) protocol = hdrPool.add(ConstStrA(splt.getNext()));
				//parse some fields
				TextParser<char,StaticAlloc<256> > parser;

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
			} else if (line.empty()) {
				return finishReadHeader();
			} else {

				natural dblcolon = line.find(':');
				if (dblcolon == naturalNull) {
					errorPage(400,ConstStrA(),"line");
					return ITCPServerConnHandler::cmdRemove;
				}
				ConstStrA field = line.head(dblcolon);
				ConstStrA value = line.offset(dblcolon+1);
				cropWhite(field);
				cropWhite(value);
				requestHdrs.insert(hdrPool.add(field),hdrPool.add(value));
			}
			pos = buffer.lookup(ConstBin("\r\n"));
		}
		return ITCPServerConnHandler::cmdWaitRead;
	} catch (AllocatorLimitException &) {
		errorPage(413,ConstStrA(),"Header is too large (4096 bytes)");
		return ITCPServerConnHandler::cmdRemove;
	}
}


void HttpReqImpl::recordRequestDuration(natural) {
	reportDuration = false;
}

ITCPServerConnHandler::Command  HttpReqImpl::finishReadHeader() {


	TextParser<char,StaticAlloc<256> > parser;

	//open output and input, we need stream to show error pages
	outputClosed = false;
	inputClosed = false;


	isHeadMethod = ConstStrA(method) == "HEAD";
	closeConn = httpMinVer == 0;

	//check connection
	HeaderValue strconn = getHeaderField(fldConnection);
	if (strconn.defined) {
		if (strconn == "close") closeConn = true;
	}


	//check content length
	HeaderValue strsize = getHeaderField(fldContentLength);
	if (strsize.defined && parser(" %u1 ", strsize)) {
		remainPostData = parser[1];
	} else {
		remainPostData = 0;
	}

	//check chunked POST
	HeaderValue strchunked = getHeaderField(fldTransferEncoding);
	if (strchunked.defined != 0 && strchunked == "chunked") {
		chunkedPost = true;
		remainPostData = 0; //remainPostData is used to count chunks
	} else {
		chunkedPost = false;
	}

	//check expectation for 100-continue
	HeaderValue strexpect = getHeaderField(fldExpect);
	if (strexpect.defined != 0) {
		if (strexpect == "100-continue" || strexpect == "100-Continue") {
			bNeedContinue = true;
		} else {
			return errorPageKA(417);
		}
	} else {
		bNeedContinue = false;
	}

	//check host
	ConstStrA vpath = path;
	host = getHeaderField(fldHost);
	if (!mapHost(host, vpath)) {
		return errorPageKA(404);
	}

	if (vpath.empty()) {
		redirect("+/");
		return processHandlerResponse(0);
	}


	//find handler
	IHttpHandler *h;
	curHandler = nil;
	natural res = callHandler( vpath, &h);
	if (h == 0) return errorPageKA(404);
	if (curHandler == nil) curHandler = h; //need this to correctly handle forward function
	return processHandlerResponse(res);
}

bool HttpReqImpl::isInputAvailable() const {
	if (chunkedPost) return !inputClosed;
	else return switchedProtocol || remainPostData > 0;
}

ITCPServerConnHandler::Command  HttpReqImpl::processHandlerResponse(natural res) {
	//stDetach causes waiting for user wakeup.
	if (res == IHttpHandler::stSleep) {
		return ITCPServerConnHandler::cmdWaitUserWakeup;
	}
	//depend on whether headers has been sent
	if (bHeaderSent) {
		//in case of 100 response, keep connection on-line
		if (res == 100) {
			if (!isInputAvailable()) {
				evType = evEndOfStream;
				res = curHandler->onData(*this);
				if (res == 100) res =500;
				return processHandlerResponse(res);
			}
			return ITCPServerConnHandler::cmdWaitRead;
		}
		else if (res == IHttpHandler::stWaitForWrite) {
			return ITCPServerConnHandler::cmdWaitWrite;
		} else {
			finish();
			//keep conection depend on keep-alive
			return keepAlive()?ITCPServerConnHandler::cmdWaitRead:ITCPServerConnHandler::cmdRemove;
		}
	} else {
		//in case of 100 response
		if (res == 100) {

			if (bNeedContinue) {
				send100continue();
			}

			if (!isInputAvailable()) {
				res = curHandler->onData(*this);
				if (res == 100) res =500;
				return processHandlerResponse(res);
			}
						//keep connection alive
			return ITCPServerConnHandler::cmdWaitRead;
		} else if (res == IHttpHandler::stWaitForWrite) {

			sendHeaders();
			return ITCPServerConnHandler::cmdWaitWrite;

		}
		//otherwise response with status page
		return errorPageKA(res);
	}
}

void HttpReqImpl::clear() {
	method = ConstStrA();
	requestHdrs.clear();
	responseHdrs.clear();
	hdrPool.clear();
	responseHdrPool.clear();
	statusMsg = HdrStr();
	curHandler = nil;
	bHeaderSent = false;
	isHeadMethod = false;
	useChunked = false;
	inout = 0;
	remainPostData = 0;
	chunkedPost = false;
	requestContext = nil;
	statusCode=200;
	postBodyLimit = naturalNull;
	switchedProtocol = false;
	requestName = HdrStr();

}

void HttpReqImpl::finish() {
	if (!outputClosed) {
		//otherwise finalize chunk
		finishChunk();
	}
	if (!method.empty()) {
		TimeStamp reqEndTime = TimeStamp::now();
		natural reqTime = (reqEndTime - reqBeginTime).getMilis();
		logRequest(reqTime);
		if (reportDuration) this->recordRequestDuration(reqTime);


	}
	clear();
}

ITCPServerConnHandler::Command HttpReqImpl::errorPageKA(natural code, ConstStrA expl) {
	errorPage(code,ConstStrA(),expl);
	finish();
	return ITCPServerConnHandler::cmdRemove;
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
	busySemaphore->unlock(lockStatus);
}
void HttpReqImpl::endIO() {
	busySemaphore->lock(lockStatus);
}

PNetworkStream HttpReqImpl::getConnection() {
	inout->flush();
	return inout->getHandle().getMT();
}

void HttpReqImpl::setMaxPostSize(natural bytes) {
	if (remainPostData > bytes) throw HttpStatusException(THISLOCATION,path,413,"Request is too large");
	postBodyLimit = bytes;

}

bool HttpReqImpl::mapHost(ConstStrA , ConstStrA &)
{

	//base implementation doesn't support host mapping, so function
	//doesn't change anything and just returns success - this makes 1:1
	//mapping despite on which host was specified.
	return true;
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

void HttpReqImpl::setRequestName(ConstStrA name) {
	requestName = responseHdrPool.add(name);
}

void HttpReqImpl::logRequest(natural reqTime)
{
	LogObject(THISLOCATION).progress("%7 - %10 %3 %4 HTTP/%1.%2 %5 %6 %8 (%9 ms)")
		<< httpMajVer << httpMajVer << ConstStrA(method)
		<< ConstStrA(path) << statusCode << statusMsg
		<< getIfc<IHttpPeerInfo>().getPeerRealAddr()
		<< requestName
		<< reqTime
		<< host;
}

IHttpRequest::EventType HttpReqImpl::getEventType() {
	return evType;
}

natural HttpReqImpl::getWakeupReason() {
	return wakeUpReason;
}

}

