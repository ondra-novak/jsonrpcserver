/*

 * httpClient.cpp
 *
 *  Created on: 29. 4. 2016
 *      Author: ondra
 */

#include <lightspeed/base/text/textParser.tcc>
#include "httpClient.h"

#include "lightspeed/base/exceptions/httpStatusException.h"
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

HttpClient::HdrItem::HdrItem(ConstStrA field, ConstStrA value, const HdrItem* next)
	:field(field),value(value),next(next) {}

HttpClient::HdrItem::HdrItem(ConstStrA field, const StringCore<char>& value, const HdrItem* next)
	:field(field),value(value),next(next) {}

HttpClient::HdrItem::HdrItem(ConstStrA field,const char* value, const HdrItem* next)
	:field(field),value(ConstStrA(value)),next(next) {}

HttpClient::HdrItem::HdrItem(Field field, ConstStrA value, const HdrItem* next)
	:field(HttpClient::getHeaderFieldName(field)),value(value),next(next) {}

HttpClient::HdrItem::HdrItem(Field field,const StringCore<char>& value, const HdrItem* next)
	:field(HttpClient::getHeaderFieldName(field)),value(value),next(next) {}

HttpClient::HdrItem::HdrItem(Field field, const char* value, const HdrItem* next)
:field(HttpClient::getHeaderFieldName(field)),value(ConstStrA(value)),next(next) {}

HttpResponse& HttpClient::getContent(ConstStrA url, const HdrItem* headers) {
}

HttpResponse& HttpClient::sendRequest(ConstStrA url, Method method, ConstBin data, const HdrItem* headers) {
}

HttpResponse& HttpClient::sendRequest(ConstStrA url, Method method, SeqFileInput data, ConstStringT<ConstStrA> headers) {
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

HttpRequest& HttpClient::createRequest(ConstStrA url, Method method) {

	TextParser<char, SmallAlloc<1024> > parser;
	if (parser("http%(?)[s]1://%(*)*2%%[a-zA-Z0-9-_.:]3/%(*)4",url)) {
		ConstStrA s = parser[1].str();
		ConstStrA auth = parser[2].str();
		ConstStrA domain_port = parser[3].str();
		ConstStrA path = parser[4].str();
		path = url.map(path);
		path = ConstStrA(path.data()-1, path.length()+1);
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

		if (tls) {
			if (httpsProvider == 0) {
				throw InvalidUrlException(THISLOCATION,url,"Https is not configured");
			}
			if (!canReuseConnection(domain_port, tls)) {
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
				response->skipRemainBody();
			}
		} else {
			if (proxyInfo.defined) {
				if (!canReuseConnection(domain_port,tls)) {
					PNetworkStream stream;
					stream = connectSite(proxyInfo.proxyAddr,8080);
					nstream = new BufferedNetworkStream(stream);
					path = url;
				} else {
					if (response != nil) {
						response->skipRemainBody();
					}
				}
			} else {
				if (!canReuseConnection(domain_port,tls)) {
					PNetworkStream stream;
					stream = connectSite(domain_port,80);
					nstream = new BufferedNetworkStream(stream);
				} else {
					if (response != nil) {
						response->skipRemainBody();
					}
				}
			}
		}
		response = nil;
		request = nil;
		request = Constructor4<HttpRequest,IOutputStream *,ConstStrA,Method ,bool>(
				nstream.get(), path, method, !useHTTP10);
		if (proxyInfo.defined && !tls && !proxyInfo.authorization.empty()) {
			request->setHeader(fldProxyAuthorization, proxyInfo.authorization);
		}
		request->setHeader(fldHost, domain_port);
		request->setHeader(fldUserAgent, userAgent);
		return request;


	} else {
		throw InvalidUrlException(THISLOCATION,url,"Parser rejected");
	}



}

HttpResponse& HttpClient::createResponse(bool readHeaders) {
	if (request == null) throw ErrorMessageException(THISLOCATION,"No active request");
	if (!request->isOutputClosed()) request->closeOutput();
	request = nil;

	if (readHeaders)
		response = Constructor2<HttpResponse, IInputStream *, HttpResponse::ReadHeaders>(nstream.get(),HttpResponse::readHeadersNow);
	else
		response = Constructor1<HttpResponse, IInputStream *>(nstream.get());
	return response;
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
	HttpResponse resp(stream.get(),HttpResponse::readHeadersNow);
	if (resp.getStatus() != 200)
		throw HttpStatusException(THISLOCATION,host,resp.getStatus(), resp.getStatusMessage());
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


} /* namespace BredyHttpClient */
