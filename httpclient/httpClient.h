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

#include "lightspeed/base/containers/stringKey.h"

#include "lightspeed/base/streams/netio.h"

#include "lightspeed/base/exceptions/errorMessageException.h"
#include "interfaces.h"
namespace BredyHttpClient {

using namespace LightSpeed;



struct ClientConfig {
	///userAgent user agent identification (string passed to every request)
	ConstStrA userAgent;
	///httpsProvider Pointer to a provider which provides TLS. Set NULL to disable https protocol
	IHttpsProvider *httpsProvider;
	///proxyProvider Pointer to a provider which provides proxy redirection. Set NULL to disable proxies
	IHttpProxyProvider *proxyProvider;
	///standard timeout for reading/writting, default is 30 second. Time is in miliseconds
	/** this value is used to setup connection on initial connect. You can change timeout anytime later, or
	 * perform own waiting on the stream during reading the dara
	 */
	natural iotimeout;

	///specify true to use HTTP/1.0 protocol (default is false)
	bool useHTTP10;
	///if true, client will keep connection opened across requests (defautl is false)
	bool keepAlive;

	ClientConfig();
};

class HttpClient: public BredyHttpSrv::HeaderFieldDef {
public:
	typedef NetworkStream<> NStream;

	///Constructs http client
	/**
	 * @param userAgent user agent identification (string passed to every request)
	 * @param httpsProvider Pointer to a provider which provides TLS. Set NULL to disable https protocol
	 * @param proxyProvider Pointer to a provider which provides proxy redirection. Set NULL to disable proxies
	 */
	HttpClient(const ClientConfig &cfg);


	class HdrItem {
	public:
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
	 * @param url full url to request
	 * @param method method of the request
	 * @param data data of the request. For methods without body the data can be empty
	 * @param headers
	 * @return Function returns reference to response. It already contains parsed headers and only data can be read.
	 */
	HttpResponse &sendRequest(ConstStrA url, Method method, ConstBin data = ConstBin(), const HdrItem *headers = 0);
	///Sends HTTP/S request to specified URL. You can specify method and data
	/**
	 * @param url full url to request
	 * @param method method of the request
	 * @param Stream that contains data. You can use IInputStream to write own data source
	 * @param headers
	 * @return Function returns reference to response. It already contains parsed headers and only data can be read.
	 *
	 * @note Http/1.1 is enforced because chunked transfer encoding will be used
	 */
	HttpResponse &sendRequest(ConstStrA url, Method method, SeqFileInput data, const HdrItem* headers);


	///Closes connection even if keep alive is in effect
	void closeConnection();

	///Returns current connection
	/**
	 * @retval null connection is not currently available - this can happen between requests when keepalive is not available
	 * @return other - pointer to current connection, you can adjust parameters or read data directly, however keep
	 * in eye, that transfer encoding can be applied.
	 */
	PNetworkStream getConnection();





protected:

	///Creates request object to craft custom http request
	HttpRequest &createRequest(ConstStrA url, Method method);
	///Creates response. You have to close request first before response is created
	/**
	 * Creates response object on the connection
	 *
	 * @param receiveHeaders set true to return object with already initialized headers.
	 * If you specify false, you can receive headers manually by doing series of
	 * non-blocking readings
	 * @return a response object
	 *
	 * @note you should read whole response before new call of createRequest() is made.
	 * Otherwise, function createRequest can block until the rest of response is read
	 */
	HttpResponse &createResponse(bool receiveHeaders);


	///Retrieves reuse state.
	/** The state is valid after request is created. Reuse state is state, when request has been made
	 * on a connection which has been previously used for other request with keep alive feature. This information
	 * is need to handle connection termination during creation the request or reading the response. If response
	 * fails with reuseState equal true, you should closeConnection() and repeat the request.
	 *
	 * Text above applies on function createRequest and createResponse,
	 *
	 * @return
	 */
	bool getReuseState() const {return connectionReused;}


	class BufferedNetworkStream: public IOBuffer<2048>, public INetworkStream {
	public:
		BufferedNetworkStream(const PNetworkStream &stream);

	protected:
		PNetworkStream originStream;
		virtual natural getDefaultWait() const;
		virtual void setWaitHandler(WaitHandler *handler);
		virtual WaitHandler *getWaitHandler() const;
		virtual void setTimeout(natural time_in_ms);
		virtual natural getTimeout() const;
		virtual natural wait(natural waitFor, natural timeout) const;
		virtual natural doWait(natural waitFor, natural timeout) const;
	};

	Optional<HttpRequest> request;
	Optional<HttpResponse> response;
	PNetworkStream nstream;

	StringA userAgent;
	IHttpsProvider *httpsProvider;
	IHttpProxyProvider *proxyProvider;
	bool useHTTP10;
	bool keepAlive;

	StringA currentDomain;
	bool currentTls;
	bool connectionReused;
	natural iotimeout;

	PNetworkStream connectSite(ConstStrA site, natural defaultPort);
	void proxyConnect(PNetworkStream stream, ConstStrA host, ConstStrA authorization);

private:
	bool canReuseConnection(const ConstStrA& domain_port, bool tls);
	void feedHeaders(HttpRequest& rq, const HdrItem* headers);
};

class InvalidUrlException: public ErrorMessageException {
public:
	InvalidUrlException(const ProgramLocation &loc,
			const StringA &url,const String &text) : ErrorMessageException(loc, text),url(url) {}
	ConstStrA getUrl() const {return url;}
	LIGHTSPEED_EXCEPTIONFINAL;
	~InvalidUrlException() throw();
protected:
	StringA url;
	void message(ExceptionMsg &msg) const;
};

} /* namespace BredyHttpClient */

#endif /* JSONRPCSERVER_HTTPCLIENT_HTTPCLIENT_H_23dkaei2k3409uqnsqe12 */
