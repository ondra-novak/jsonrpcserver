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
	typedef BredyHttpSrv::HeaderValue HeaderValue;

	///Options how to POST a stream
	enum PostStreamOption {
		///Default behaviour
		/** For HTTP/1.0 will defaults to psoBuffer,
		 *  For HTTP/1.1 will defaults to psoAllow100
		 */
		psoDefault,
		///Use buffering for the stream, this is only option available for HTTP/1.0
		/** Whole request is buffered and send once the stream reaches its end. Option
		 * cannot be used for infinite stream (http/1.0) doesn't support such streaming
		 */
		psoBuffer,
		///Allow of usage 100-continue, but not neceserly everytime.
		/** Function will use 100-continue to ensure, that keep-alive connection is still alive */
		psoAllow100,
		///Use 100-continue even if it is not necesery
		/** Will set 100-continue, but will switch temporaryly to psoAvoid100 when 417 is returned */
		psoFavour100,
		///Force 100-continue, report 417 error back to caller
		psoForce100,
		///Never request 100-continue.
		/** This can cause network errors for "keep-alive" connections if connection was terminated
		 * from server side during the request. If this event is detected during or after stream is posted,
		 * the request cannot be repeated because stream is lost. Then the appropriate network error will be throw out
		 * of the function
		 */
		psoAvoid100,
		///Never request 100-continue and do not reuse connection
		/**Similar to psoAvoid100, but it also closes current connection regadless on wherther
		 * can be or cannot be reused. This can solve disadvantage of keep-alive connection for psoAvoid100
		 */
		psoAvoid100NoReuseConn,
	};


	///Constructs http client
	/**
	 * @param userAgent user agent identification (string passed to every request)
	 * @param httpsProvider Pointer to a provider which provides TLS. Set NULL to disable https protocol
	 * @param proxyProvider Pointer to a provider which provides proxy redirection. Set NULL to disable proxies
	 */
	HttpClient(const ClientConfig &cfg);

	///Opens request to given url
	/**
	 * @param method HTTP method
	 * @param url full url
	 */
	void open(Method method, ConstStrA url);

	///Sets request's header value
	/**
	 * @param field field name
	 * @param value content of the field
	 */
	void setHeader(Field field, ConstStrA value);

	///Sets request's header value
	/**
	 * @param field field name
	 * @param value content of the field
	 */
	void setHeader(ConstStrA field, ConstStrA value);

	///Starts body of POST request
	/**
	 *
	 * @param pso prefered way how to send the body. It controls usage of Expect: 100-continue. For
	 * http/1.0, only psoBuffer is allowed because http/1.0 doesn't support streaming for POST request
	 * @return stream where caller should put body
	 *
	 * @note after body is started, the header is sent to the stream and you can no longer modify it. Also
	 * note, that in this stage, connection is online. Server on other side can anytime close connection
	 * without any response (exception is thrown)
	 *
	 * @exception HttpStatusException function throws this exception when server rejects the body. This
	 * can
	 *
	 */
	SeqFileOutput beginBody(PostStreamOption pso = psoDefault);


	///Sends request with or without body
	/** If you wish to send request with body, you should first call beginBody() to open body. If body
	 * is present, function just finishes the request and starts processing the response.
	 * @return input stream, contains response.
	 */
	SeqFileInput send();

	///Sends request with body passed as argument
	/**
	 *
	 * @param body content of body
	 * @return input stream, contains response.
	 *
	 * @note if you previously opened the body, function ignores the argument
	 */
	SeqFileInput send(ConstStrA body);


	///Sends request without waiting on response. You need to call receiveResponse to receive the response
	void sendNoWait();

	///Receives response after the request has been send using sendAsync()
	/** @return input stream, contains response.*/
	SeqFileInput receiveResponse();

	///Retrieves status message of the response. You need to first send the request to receive response
	natural getStatus() const;

	///Retrieves status message
	ConstStrA getStatusMessage() const;

	///Retrieves header field
	HeaderValue getHeader(Field field) const;


	///Retrieves header field
	HeaderValue getHeader(ConstStrA field) const;




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


	void sendRequest(natural contentLength, PostStreamOption pso);

	void loadResponse();

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

	HttpResponse &sendRequestInternal(ConstStrA url, Method method, SeqFileInput data, const HdrItem* headers, bool wo100 = false);


	StringPool<char> strpool;
	typedef StringPoolStrRef<char> StrRef;
	typedef Map<StrRef, StrRef> HdrMap;

	StrRef urlToOpen;
	Method methodToOpen;
	HdrMap hdrMap;
	natural status;
	StrRef statusMessage;




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
