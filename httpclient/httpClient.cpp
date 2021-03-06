/*

 * httpClient.cpp
 *
 *  Created on: 29. 4. 2016
 *      Author: ondra
 */

#include <lightspeed/base/text/textParser.tcc>
#include "httpClient.h"

#include "lightspeed/base/exceptions/httpStatusException.h"

#include "lightspeed/base/exceptions/netExceptions.h"
#include <lightspeed/base/exceptions/fileExceptions.h>
#include "lightspeed/base/streams/fileiobuff.tcc"
#include "lightspeed/base/containers/map.tcc"

namespace BredyHttpClient {

ClientConfig::ClientConfig()
:userAgent("BredyHttpClient/1.0 (+https://github.com/ondra-novak/jsonrpcserver)")
,httpsProvider(0)
,proxyProvider(0)
,iotimeout(30000)
,useHTTP10(false)
,keepAlive(false)
{
}

HttpClient::HttpClient(const ClientConfig& cfg)
	:userAgent(cfg.userAgent)
	,httpsProvider(cfg.httpsProvider)
	,proxyProvider(cfg.proxyProvider)
	,useHTTP10(cfg.useHTTP10)
	,keepAlive(cfg.keepAlive)
	,iotimeout(cfg.iotimeout)
{
}

#if 0
HttpResponse& HttpClient::sendRequest(ConstStrA url, Method method, ConstBin data, const HdrItem* headers) {
	//sending request with string body is quite easy
	try {
		//create request
		HttpRequest &rq = createRequest(url,method);
		//set content length
		rq.setContentLength(data.length());
		//put headers
		feedHeaders(rq, headers);
		//write body
		rq.writeAll(data.data(),data.length());
		//close output
		rq.closeOutput();
		//and receive response
		return createResponse(true);
	} catch (const NetworkException &e) {
		//if network error happened and connection has been reused
		if (connectionReused) {
			//close the connection (connectionReuse is set to false)
			closeConnection();
			//repeate the request
			return sendRequest(url, method, data, headers);
		} else {
			//throw other errors
			throw;
		}
	}

}

HttpResponse& HttpClient::sendRequest(ConstStrA url, Method method, SeqFileInput data, const HdrItem* headers) {

	return sendRequestInternal(url, method,data,headers,false);
}

#endif

void HttpClient::open(Method method, ConstStrA url) {

	strpool.clear();
	respHdrMap.clear();
	reqHdrMap.clear();
	urlToOpen = strpool.add(url);
	methodToOpen = method;
}

void HttpClient::setHeader(Field field, ConstStrA value) {
	reqHdrMap.replace(strpool.add(getHeaderFieldName(field)),strpool.add(value));
}

void HttpClient::setHeader(ConstStrA field, ConstStrA value) {
	reqHdrMap.replace(strpool.add(field),strpool.add(value));
}

SeqFileOutput HttpClient::beginBody(PostStreamOption pso) {
	sendRequest(naturalNull, pso);
	return SeqFileOutput(request.get());
}

SeqFileInput HttpClient::send() {
	if (request == null) {
		try {
			sendRequest(naturalNull, psoAvoid100);
			request->closeOutput();
			loadResponse();
		} catch (NetworkException &) {
			if (connectionReused) {
				closeConnection();
				return send();
			}
			throw;
		} catch (LightSpeed::IOException &) {
			if (connectionReused) {
				closeConnection();
				return send();
			}
			throw;
		}
	} else {
		request->closeOutput();
		loadResponse();
	}
	return SeqFileInput(response.get());
}

SeqFileInput HttpClient::send(ConstStrA body) {
	if (request == null) {
		try {
			sendRequest(body.length(), psoAvoid100);
			request->writeAll(body.data(), body.length());
			request->closeOutput();
			loadResponse();
			return SeqFileInput(response.get());
		} catch (NetworkException &) {
			if (connectionReused) {
				closeConnection();
				return send(body);
			}
			throw;
		}
	} else {
		return send();
	}
}

void HttpClient::sendNoWait() {
	if (request == null) {
		try {
			sendRequest(naturalNull, psoAvoid100);
			request->closeOutput();
		} catch (NetworkException &) {
			if (connectionReused) {
				closeConnection();
				return sendNoWait();
			}
		}
	} else {
		request->closeOutput();
	}
}

SeqFileInput HttpClient::receiveResponse() {
	loadResponse();
	return SeqFileInput(response.get());
}

natural HttpClient::getStatus() const {
	return status;
}

ConstStrA HttpClient::getStatusMessage() const {
	return statusMessage;
}

HttpClient::HeaderValue HttpClient::getHeader(Field field) const {
	return getHeader(getHeaderFieldName(field));
}

HttpClient::HeaderValue HttpClient::getHeader(ConstStrA field) const {
	const StrRef *strr = respHdrMap.find(field);
	if (strr == 0) return HeaderValue();
	return HeaderValue(*strr);
}

static ConstStrA continue100("100-continue");

void HttpClient::sendRequest(natural contentLength, PostStreamOption pso) {
	try {
		createRequest(urlToOpen,methodToOpen);
		request->setContentLength(contentLength);
		bool use100 = false;
		if (!useHTTP10) {


			switch(pso) {
			case psoDefault:
			case psoAllow100: use100 = connectionReused; break;
			case psoFavour100:
			case psoForce100: use100 = true;break;
			case psoBuffer: request->setContentLength(HttpRequest::calculateLength);break;
			case psoAvoid100NoReuseConn: if (connectionReused) {
											closeConnection();
											createRequest(urlToOpen,methodToOpen);
										};break;
			case psoAvoid100: break;
			}

			if (use100) {
				setHeader(fldExpect,continue100);
			}
		}

		for (HdrMap::Iterator iter = reqHdrMap.getFwIter(); iter.hasItems();) {
			const HdrMap::KeyValue &kv = iter.getNext();
			request->setHeader(kv.key, kv.value);
		}


		request->beginBody();
		createResponse(false);

		if (use100) {
			//flush buffers now
			nstream->flush();
			if (nstream->wait(INetworkResource::waitForInput,10000) != INetworkResource::waitTimeout) {
				response->readHeaders();
				//there can other response, so if we cannot continue, return response to the caller
				if (!response->canContinue()) {
					//check status
					if (status == 417) {//we got precondition failed - no worries, we can repeat the request

						response->skipRemainBody(false);

						return sendRequest(contentLength, psoAvoid100);

					} else {

						loadResponse();
						throw HttpStatusException(THISLOCATION,urlToOpen,status, statusMessage);

					}
				}
			}
		}


	}catch (NetworkException &) {
		if (connectionReused) {
			closeConnection();
			sendRequest(contentLength,pso);
		} else {
			throw;
		}
	}catch (IOException &) {
		if (connectionReused) {
			closeConnection();
			sendRequest(contentLength,pso);
		} else {
			throw;
		}
	}
}

void HttpClient::loadResponse() {

	if (request == null) {
		throw FileIOError(THISLOCATION, 1, "Can't read response without request");
	}

	if (!request->isOutputClosed()) request->closeOutput();

	if (response == null) {
		createResponse(true);
	} else {
		response->readHeaders();
	}


	while (response->canContinue()) {
		response->waitAfterContinue(HttpResponse::readHeadersNow);
	}

	request = null;
}

#if 0

HttpResponse& HttpClient::sendRequestInternal(ConstStrA url, Method method, SeqFileInput data, const HdrItem* headers, bool wo100) {

	//for HTTP/1.0 we need to buffer request (sad)
	if (useHTTP10) {
		//create buffer
		AutoArray<byte> buffer;
		if (data.hasItems()) {
			do {
				natural pos = buffer.length();
				//reserve buffer
				buffer.resize(pos+4096);
				//read buffer
				natural sz = data.blockRead(buffer.data()+pos,4096,true);
				//adjust buffer size
				buffer.resize(pos+sz);
				//continue until end
			} while (data.hasItems());
		}
		//send request
		return sendRequest(url,method,buffer,headers);
	} else {
		//while reading from the stream, we need to ensure, that server is ready to accept our data
		//so in this case, we will use Expect: 100-continue precondition
		try {
			//create request for url
			HttpRequest &rq = createRequest(url,method);
			//we don't need to use 100-continue if connection was recently opened
			if (!connectionReused) wo100 = true;
			//set content length to infinity - this should switch to chunked
			rq.setContentLength(naturalNull);
			//load headers
			feedHeaders(rq, headers);
			//set Expect: 100-continue
			if (!wo100) rq.setHeader(fldExpect,"100-continue");
			//close headers and start body, but we must wait for response now
			rq.beginBody();
			//so create response (do not read headers)
			HttpResponse& resp = createResponse(false);
			//now wait some time to server's response.
			//because it could ignore our header, we must not wait for infinity time
			if (wo100 || nstream->wait(INetworkResource::waitForInput,10000) != INetworkResource::waitTimeout) {
				//data arrived in time - read headers
				resp.readHeaders();
				//there can other response, so if we cannot continue, return response to the caller
				if (!resp.canContinue()) {
					//check status
					if (resp.getStatus() == 417) //we got precondition failed - no worries, we can repeat the request
						//at least we know, that connection is alive
						return sendRequestInternal(url,method,data,headers,true);
					else {
						//other status is send to the caller
						return resp;
					}
				}
			}

			//until now, request could be repeated anytime for network errors
			//- because keep-alive can be closed by the server anytime during request

			//now we should prevent repeating request when network error happened
			//because we starting reading the stream
			connectionReused = false;

			//create a buffer for data
			CArray<byte, 1024> buffer;
			//open request as stream
			SeqFileOutput dout(&rq);
			//use blockcopy to copy data to the request
			dout.blockCopy(data,buffer,naturalNull);
			//close the output
			rq.closeOutput();
			//now wait for response.
			resp.waitAfterContinue(HttpResponse::readHeadersNow);

			//because we could skip waiting on 100-continue above, we need to receive it now
			//and any other such responses
			while (resp.canContinue()) {
				resp.waitAfterContinue(HttpResponse::readHeadersNow);
			}

			//return other response to the caller
			return resp;
		} catch (const NetworkException &e) {
			if (connectionReused) {
				closeConnection();
				return sendRequest(url, method, data, headers);
			} else {
				throw;
			}
		}
	}
}

#endif

void HttpClient::closeConnection() {
	nstream = nil;
	request = nil;
	response = nil;
}


bool HttpClient::canReuseConnection(const ConstStrA& domain_port, bool tls) {
	//we can reuse connection when
	//host doesn't changes
	return domain_port == currentDomain
			//we have same encryption mode
			&& tls == currentTls
			//there is no opened request (with active request, we don't know state of connection)
			&& request == nil
			//there is active response (previous one)
			&& response != nil
			//active response has keepAlive status
			&& response->isKeepAliveEnabled();
}

void HttpClient::createRequest(ConstStrA url, Method method) {

	connectionReused = false;
		TextParser<char, SmallAlloc<1024> > parser;
		if (parser("http%(?)[s]1://%(*)*2%%[a-zA-Z0-9-_.:]3/%(*)4",url)) {
			ConstStrA s = parser[1].str();
			ConstStrA auth = parser[2].str();
			ConstStrA domain_port = parser[3].str();
			ConstStrA path = parser[4].str();
			path = url.tail(path.length()+1);
			if (!s.empty() && s != "s")
				throw InvalidUrlException(THISLOCATION,url,"unknown protocol");
			if (!auth.empty() && s.tail(1)[0] != '@')
				throw InvalidUrlException(THISLOCATION,url,"auth need '@' as separator");
			if (domain_port.tail(1)[0] == ':')
				throw InvalidUrlException(THISLOCATION,url,"missing port number");

			bool tls = !s.empty();
			IHttpProxyProvider::Result proxyInfo;
			if (proxyProvider) {
				proxyInfo = proxyProvider->getProxy(domain_port);
			}

			if (canReuseConnection(domain_port, tls)) {
				try {
					response->skipRemainBody();
					connectionReused = true;
				} catch (const NetworkException &) {
					response = nil;
					nstream = nil;
				}
			}

			if (!connectionReused) {
				if (tls) {
					if (httpsProvider == 0) {
						throw InvalidUrlException(THISLOCATION,url,"Https is not configured");
					}
					PNetworkStream stream;
					if (proxyInfo.defined) {
						stream = connectSite(proxyInfo.proxyAddr,8080);
						proxyConnect(stream, domain_port, proxyInfo.authorization);
					} else {
						stream = connectSite(domain_port, 443);
					}
					stream = httpsProvider->connectTLS(stream,domain_port);
					nstream = new BufferedNetworkStream(stream);
					currentDomain = domain_port;
					currentTls = true;
				} else {
					if (proxyInfo.defined) {
						PNetworkStream stream;
						stream = connectSite(proxyInfo.proxyAddr,8080);
						nstream = new BufferedNetworkStream(stream);
						path = url;
						currentDomain = proxyInfo.proxyAddr;
					} else {
						PNetworkStream stream;
						stream = connectSite(domain_port,80);
						nstream = new BufferedNetworkStream(stream);
						currentDomain = domain_port;
					}
				}
			}
			response = nil;
			request = new(pool) HttpRequest(nstream.get(),path,method,!useHTTP10);
			if (proxyInfo.defined && !tls && !proxyInfo.authorization.empty()) {
				request->setHeader(fldProxyAuthorization, proxyInfo.authorization);
			}
			request->setHeader(fldHost, domain_port);
			request->setHeader(fldUserAgent, userAgent);
			if (keepAlive == false) {
				request->setHeader(fldConnection,"close");
			} else if (useHTTP10) {
				request->setHeader(fldConnection,"keep-alive");
			}

		} else {
			throw InvalidUrlException(THISLOCATION,url,"Parser rejected");
		}
}

void HttpClient::createResponse(bool readHeaders) {
	if (request == null) throw ErrorMessageException(THISLOCATION,"No active request");

	if (readHeaders)
		response = new(pool) HttpResponse(nstream.get(),*this,HttpResponse::readHeadersNow);
	else
		response = new(pool) HttpResponse(nstream.get(),*this);
}

InvalidUrlException::~InvalidUrlException() throw () {
}

void InvalidUrlException::message(ExceptionMsg& msg) const  {
	msg("Invalid url: %1 - %2") << url << this->textDesc;
}

PNetworkStream HttpClient::getConnection() {
	return nstream;
}

PNetworkStream HttpClient::connectSite(ConstStrA site, natural defaultPort) {
	NetworkAddress addr(site, defaultPort);
	NetworkStreamSource streamSource(addr,1,iotimeout,iotimeout);
	PNetworkStream stream = streamSource.getNext();
	return stream;
}

void HttpClient::proxyConnect(PNetworkStream stream, ConstStrA host, ConstStrA authorization) {
	if (host.find(':') == naturalNull) {
		return proxyConnect(stream,StringA(host+ConstStrA(":443")),authorization);
	}
	PNetworkStream nstream = new BufferedNetworkStream(stream);

	HttpRequest req(stream.get(), host, mCONNECT, !useHTTP10);
	if (!authorization.empty()) {
		req.setHeader(fldProxyAuthorization,authorization);
	}
	req.closeOutput();

	class ResponseResult: public IHttpResponseCB {
	public:
		virtual void storeStatus(natural statusCode, ConstStrA statusMessage) {
			status = statusCode;
			statusMsg.append(statusMessage);
		}
		virtual void storeHeaderLine(ConstStrA, ConstStrA) {

		}

		natural status;
		AutoArray<char, SmallAlloc<32> > statusMsg;
	};

	ResponseResult res;

	HttpResponse resp(stream.get(),res,HttpResponse::readHeadersNow);
	if (status != 200)
		throw HttpStatusException(THISLOCATION,host,res.status, res.statusMsg);
}


HttpClient::BufferedNetworkStream::BufferedNetworkStream(const PNetworkStream& stream)
	:IOBuffer(stream.get()), originStream(stream)
{

}

natural HttpClient::BufferedNetworkStream::getDefaultWait() const {
	return originStream->getDefaultWait();
}

void HttpClient::BufferedNetworkStream::setWaitHandler(WaitHandler* handler) {
	originStream->setWaitHandler(handler);
}

INetworkResource::WaitHandler* HttpClient::BufferedNetworkStream::getWaitHandler() const {
	return originStream->getWaitHandler();
}

void HttpClient::BufferedNetworkStream::setTimeout(natural time_in_ms) {
	originStream->setTimeout(time_in_ms);
}

natural HttpClient::BufferedNetworkStream::getTimeout() const {
	return originStream->getTimeout();
}

natural HttpClient::BufferedNetworkStream::wait(natural waitFor, natural timeout) const {
	if ((waitFor & waitForInput) && this->dataReady()) return waitForInput;
	if ((waitFor & waitForOutput) &&  this->getWriteSpace()>0) return waitForOutput;

	WaitHandler *wh = getWaitHandler();
	if (wh) {
		return wh->wait(this,waitFor,timeout);
	} else {
		return doWait(waitFor,timeout);
	}
}

natural HttpClient::BufferedNetworkStream::doWait(natural waitFor, natural timeout) const {
	return originStream->wait(waitFor, timeout);
}

void HttpClient::storeStatus(natural statusCode, ConstStrA statusMessage) {
	this->status = statusCode;
	this->statusMessage = strpool.add(statusMessage);
}

void HttpClient::close() {
	try {
		if (response != null) {
			response->skipRemainBody(false);
		}
		if (request != null) {
			closeConnection();
		}
	} catch (...) {
		closeConnection();
	}
}

void HttpClient::storeHeaderLine(ConstStrA field, ConstStrA value) {
	respHdrMap.replace(strpool.add(field),strpool.add(value));
}

void* HttpClient::BufferedNetworkStream::proxyInterface(IInterfaceRequest& p) {
	void *x = originStream->proxyInterface(p);
	if (x) return x;
	else return IInterface::proxyInterface(p);
}

const void* HttpClient::BufferedNetworkStream::proxyInterface(const IInterfaceRequest& p) const {
	const void *x = originStream->proxyInterface(p);
	if (x) return x;
	else return IInterface::proxyInterface(p);
}

} /* namespace BredyHttpClient */

