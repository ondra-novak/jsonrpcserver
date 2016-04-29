/*
 * httpClient.h
 *
 *  Created on: 29. 4. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_HTTPCLIENT_HTTPCLIENT_H_23dkaei2k3409uqnsqe12
#define JSONRPCSERVER_HTTPCLIENT_HTTPCLIENT_H_23dkaei2k3409uqnsqe12
#include "lightspeed/base/streams/netio_ifc.h"

#include "httpStream.h"
#include "lightspeed/base/containers/optional.h"
namespace BredyHttpClient {

using namespace LightSpeed;


///Basic HTTP/1.1 client doesn't support HTTPS. This object helps with creation of HTTPS connection
class IHttpsProvider {
public:

	///Performs TLS handshake and returns ssl stream
	/**
	 * @param connection connection in state being ready to TLS handshake
	 * @param hostname hostname including the port (because SNI)
	 * @return function returns decryped stream.
	 * @exception Function can throw exception when TLS handshake fails
	 */
	virtual PNetworkStream connectTLS(PNetworkStream connection, ConstStrA hostname) = 0;

};

///Allows to define proxy server for every hostname it connects
/** If proxy defined, server will connect to the proxy and uses proxy syntaxt to connect the server.
 * For HTTPS, it will use CONNECT protocol
 *
 */
class IHttpProxyProvider {
public:

	///Result of the operation
	struct Result {
		///address of the proxy in the form <domain:port>
		const ConstStrA proxyAddr;
		///Content of autorization header. If empty, no autorzation header is emitted
		const ConstStrA authorization;
		///true, if proxy is defined, otherwise false (client should connect directly)
		const bool defined;

		Result(ConstStrA proxy):proxyAddr(proxy),defined(true) {}
		Result(ConstStrA proxy, ConstStrA authorization):proxyAddr(proxy),authorization(authorization),defined(true) {}
		Result():defined(false) {}
	};
	virtual Result  getProxy(ConstStrA hostname) = 0;
};

class HttpClient: public BredyHttpSrv::HeaderFieldDef {
public:
	///Constructs http client
	/**
	 * @param userAgent user agent identification (string passed to every request)
	 * @param httpsProvider Pointer to a provider which provides TLS. Set NULL to disable https protocol
	 * @param proxyProvider Pointer to a provider which provides proxy redirection. Set NULL to disable proxies
	 */
	HttpClient(ConstStrA userAgent, IHttpsProvider *httpsProvider = 0, IHttpProxyProvider *proxyProvider = 0);


	class HdrItem {
		const ConstStrA field;
		const StringKey<StringCore<char> > value;
		const HdrItem *next;

		HdrItem(ConstStrA field, ConstStrA value, const HdrItem *next = 0);
		HdrItem(ConstStrA field, const StringCore<char> &value, const HdrItem *next = 0);
		HdrItem(ConstStrA field, const char *value, const HdrItem *next = 0);
		HdrItem(Field field, ConstStrA value, const HdrItem *next = 0);
		HdrItem(Field field, const StringCore<char> &value, const HdrItem *next = 0);
		HdrItem(Field field, const char *value, const HdrItem *next = 0);

		HdrItem operator()(ConstStrA field, ConstStrA value)  const {return HdrItem(field,value,this);}
		HdrItem operator()(ConstStrA field, const StringCore<char> &value) const {return HdrItem(field,value,this);}
		HdrItem operator()(ConstStrA field, const char *value) const {return HdrItem(field,value,this);}
		HdrItem operator()(Field field, ConstStrA value) const {return HdrItem(field,value,this);}
		HdrItem operator()(Field field, const StringCore<char> &value) const {return HdrItem(field,value,this);}
		HdrItem operator()(Field field, const char *value) const {return HdrItem(field,value,this);}

	};

	///Retrieve response to single GET request
	/**
	 * @param url URL to retrieve
	 * @param linked list of headers. It can be defined using HdrItem by concatenating headers. If NULL, none extra headers posted
	 *     &HdrItem("Content-Type","text/plain")("Accept","application/json")("Accept-Language","cs").
	 *
	 * @return Function returns reference to response. It already contains parsed headers and only data can be read.
	 *
	 * @note Object handles only one response at time. Calling another getContent() discards any unread data of previous
	 * request.
	 */

	HttpResponse &getContent(ConstStrA url, const HdrItem *headers = 0);
	///Sends HTTP/S request to specified URL. You can specify method and data
	/**
	HttpResponse &sendRequest(ConstStrA url, ConstStrA method, ConstBin data, ConstStringT<ConstStrA> headers);
	///
	HttpResponse &sendRequest(ConstStrA url, ConstStrA method, SeqFileInput data, ConstStringT<ConstStrA> headers);


	HttpRequest &createRequest(ConstStrA url);
	HttpResponse &createResponse();


protected:

	Optional<HttpRequest> request;
	Optional<HttpResponse> response;

};

} /* namespace BredyHttpClient */

#endif /* JSONRPCSERVER_HTTPCLIENT_HTTPCLIENT_H_23dkaei2k3409uqnsqe12 */
