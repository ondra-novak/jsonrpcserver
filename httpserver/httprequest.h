/*
 * httprequest.h
 *
 *  Created on: Sep 5, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_HTTPREQUEST_H_
#define JSONRPCSERVER_HTTPREQUEST_H_
#include "lightspeed/base/containers/constStr.h"
#include <utility>
#include "lightspeed/base/streams/fileio_ifc.h"
#include "lightspeed/base/actions/message.h"
#include "lightspeed/base/streams/netio_ifc.h"

namespace LightSpeed {
	class NetworkAddress;
}

namespace BredyHttpSrv {

using namespace LightSpeed;

    class IHttpHandler;

    class IHttpHandlerContext: public DynObject {
    public:
    	virtual ~IHttpHandlerContext() {}
    };

    ///Request object represents state of server for current request
    /** The object contains information about single request. But you can receive more information
     * from the object using Interface::getIfc() method. Object implements following additional interfaces:
     *
     * IHTTPMapper, IHttpPeerInfo, IHttpLiveLog, IJobScheduler, IJobManager, ITCPServerConnControl
     *

     */
	class IHttpRequest: public ISeqFileHandle {

	public:

		enum HeaderField {
			fldHost = 0,
			fldUserAgent,
			fldServer,
			fldContentType,
			fldContentLength,
			fldConnection,
			fldCookie,
			fldAccept,
			fldCacheControl,
			fldDate,
			fldReferer,
			fldAllow,
			fldContentDisposition,
			fldExpires,
			fldLastModified,
			fldLocation,
			fldPragma,
			fldRefresh,
			fldSetCookie,
			fldWWWAuthenticate,
			fldAuthorization,
			fldWarning,
			fldAccessControlAllowOrigin,
			fldETag,
			fldIfNoneMatch,
			fldIfModifiedSince,
			fldTransferEncoding,
			fldExpect,
			fldUnknown,
			fldUpgrade,
			fldAccessControlAllowMethods


		};

		class HeaderValue: public ConstStrA {
		public:
			const bool defined;
			HeaderValue():defined(false) {}
			HeaderValue(ConstStrA value):ConstStrA(value),defined(true) {}
		};

		typedef std::pair<ConstStrA,ConstStrA> HeaderFieldPair;
		typedef Message<bool,HeaderFieldPair > HdrEnumFn;

		///retrieves request method
		/**
		 * @retval GET this is ordinary GET method
		 * @retval POST post method, form data can be read by getPostData()
		 * @retval HEAD head method. Handler can process as GET method, but
		 * 	  no output will be emited (only headers). this is directly handled
		 * 	  by method write()
		 * @retval PUT method PUT. Handler must check Length and size of data
		 *    returned by GetPostData(). Additional data must be read by
		 *    read function. In this case canRead will return true.
		 */
		virtual ConstStrA getMethod() = 0;
		///Retrieves path requested by header
		virtual ConstStrA getPath() = 0;
		///Retrieves protocol
		/**
		 * @retval HTTP/1.0 request in HTTP/1.0 protocol
		 * @retval HTTP/1.1 request in HTTP/1.1 protocol
		 */
		virtual ConstStrA getProtocol() = 0;
		///Retrieves header field
		/**
		 * @param field header field name. Note that field names are case sensitive
		 * @return HeaderValue object which is compatible with ConstStrA. It contains member variable
		 *  "defined" which is true when field is defined. If "defined" is false, header
		 *  doesn't exists
		 */
		virtual HeaderValue getHeaderField(ConstStrA field) const = 0;
		///Retrieves header field
		/**
		 * @param field one of standard field defined by HeaderField enum
		 * @return HeaderValue object which is compatible with ConstStrA. It contains member variable
		 *  "defined" which is true when field is defined. If "defined" is false, header
		 *  doesn't exists
		 */
		virtual HeaderValue getHeaderField(HeaderField field) const = 0;

		///Processes all headers
		/**
		 * @param fn function packed into Message object. It will receive HeaderFieldPair which contains pair of field name and content. Function
		 *  can return true to stop processing or false to continue enumeration
		 * @retval true function stopped, probably found header that need or process all informations that requested
		 * @retval false enumeration reached final record and stops
		 */
		virtual bool enumHeader(HdrEnumFn fn) const = 0;
		///Sends all headers
		/**
		 * Function ensures, that all headers has been sent. If works only first time. Any other call causes no action.
		 *
		 * Headers are automatically sent on first usage of write() function.
		 */
		virtual void sendHeaders() =0;
		///Appends new header
		/**
		 * @param field field name
		 * @param value content of header
		 * @note function doesn't check existence of the same header. It also doesn't replace existing headers. Calling this function twice to
		 * same header field causes, that field appear twice on the reply.
		 */
		virtual void header(ConstStrA field, ConstStrA value) = 0;
		///Appends new header
		/**
		 * @param field one of standard headers
		 * @param value content of header
		 * @note function doesn't check existence of the same header. It also doesn't replace existing headers. Calling this function twice to
		 * same header field causes, that field appear twice on the reply.
		 *
		 * @note Header can
		 */
		virtual void header(HeaderField field, ConstStrA value) = 0;
		///Specify status code
		/**
		 * You can call function multiple times. Parameters of the last code will be used.
		 *
		 * @param code HTTP status code.
		 * @param msg optional message explaining the status. If string is empty, standard message is used defined by code. For example, if
		 *   code is 404, and msg is empty, "Not Found" is used as status message.
		 *
		 * @note handler can return false which is interpreted as 404. If handler exits with exception, 500 is returned. This applied only when
		 * headers has not been sent yet
		 */
		virtual void status(natural code, ConstStrA msg = ConstStrA()) = 0;
		/**
		 * Shows standard error page
		 * @param code http code
		 * @param msg message (see: status() )
		 * @param expl explanation, which appear on page
		 *
		 * After error page is shown, handler should exit immediatelly. No output should be emitted.
		 *
		 * @note function ignores headers and status code set by functions header() and status(). It uses own headers.
		 */
		virtual void errorPage(natural code, ConstStrA msg = ConstStrA(), ConstStrA expl = ConstStrA()) = 0;
		///Performs redirect
		/**
		 * @param url new url where to redirect
		 * @param code define code use with the redirect. If default value used, temporary redirect is used. For pernament redirect use code 301
		 *
		 * @note argument url can be relative. In that case, server's baseURL
		 * is used to compute absolute path
		 *
		 * @see getBaseURL
		 */
		virtual void redirect(ConstStrA url, int code = 0) = 0;

		///Retrieves base url (url for server's root)
		/** Base url can be different than expected especially when
		 * server runs behind the reverse proxy.
		 * @return
		 */
		virtual ConstStrA getBaseUrl() const = 0;
		///Enables HTTP/1.1
		/**
		 * By default server send replies in HTTP/1.0 protocol
		 *
		 * @param use true to enable http/1.1
		 */
		virtual void useHTTP11(bool use) = 0;
		///Tests whether text is an standard field
		/**
		 * @param text text to test
		 * @param fld one of standard fields
		 * @return true same
		 * @retval false not same
		 */
		virtual bool isField(ConstStrA text, HeaderField fld) const = 0;

		///Reads from the input stream
		/**
		 * You need to read stream in case that POST or PUT method is requested. This is the only way, how to
		 * read input data
		 *
		 * @param buffer buffer to store data
		 * @param size  size of data
		 * @return count of bytes really read
		 *
		 * Note that returned value can be less than requested. This can be result of slow network, when packet
		 * are smaller than buffer. If input stream is chunked (HTTP/1.1) server cannot read across chunks. It
		 * first finishes one chunk and then reads another. So reading always stops at the end of chunk. Repeated reading
		 * opens new chunk.
		 *
		 * Because interface extends ISeqFileHandle, it can be used to create SeqFileInput to perform reads. You can use SeqFileInBuff, but this will not
		 * speedup more, because network streams are also buffered.
		 */
		virtual natural read(void *buffer,  natural size) = 0;
		///Writes into output stream
		/**
		 *
		 * @param buffer buffer contains data to write
		 * @param size size of the buffer
		 * @return count of bytes really written
		 *
		 * @note first write causes flushing headers
		 *
		 * Because interface extends ISeqFileHandle, it can be used to create SeqFileOutput to perform writes. You can use SeqFileOutBuff, but this will not
		 * speedup more, because network streams are also buffered.
		 */
        virtual natural write(const void *buffer,  natural size) = 0;
        ///Peeks input buffer
        /**
         * @param buffer pointer to buffer
         * @param size size of buffer
         * @return count of bytes written into buffer
         *
         * @note specification says, that function must store at least one byte. This can cause blocking while waiting to it. Also note, that function
         *  don't need to fill whole buffer. Some streams are able to return only the first byte and not more.
         */
		virtual natural peek(void *buffer, natural size) const = 0;
		virtual bool canRead() const = 0;
		virtual bool canWrite() const = 0;
		virtual void flush() = 0;
		virtual natural dataReady() const = 0;

		///finishes current request
		/** do not call inside handler. It is designed to be called outside
		 * of the handler asynchronously.
		 *
		 * Function finishes current chunk and closes connection
		 */
		virtual void finish() = 0;

		///Tests, whether headers has been sent.
		/**
		 * @retval true headers has been already sent. You cannot modify them
		 * @retval false headers has not been sent yet. You can add headers or change status code
		 * @return
		 */
		virtual bool headersSent() const = 0;
		///Allows to call handler with new path
		/**Function is useful to handle path rewrittng
		 *
		 * @param path new path. Handler will be choosen by mapping depend of this argument
		 * @retval true handler has been executed
		 * @retval false no handler found or all found handlers rejected the request
		 *
		 * @note current handler will not be executed again recursively
		 */
		virtual natural callHandler(ConstStrA path, IHttpHandler **h = 0) = 0;

		///Allows to call handler with modified request
		/**
		 * Caller must implement own version of IHttpRequest. Function getPath() should return new path to perform handler mapping
		 * @param request modified request. Class may refer to original request object. Note that handler will use write() functions to emit output. This
		 * allows to implement translators, buffering, and so on.
		 * @return true handler has been executed
		 * @retval false no handler found or all found handlers rejected the request
		 * @note current handler will not be executed again recursively
		 */
		virtual natural callHandler(IHttpRequest &request, IHttpHandler **h = 0) = 0;


		virtual natural forwardRequest(ConstStrA path) = 0;

		///Contains information whether caller should close connection after request is processed
		/**
		 *
		 * @retval true connection will be kept alive. This result is returned when
		 *   HTTP/1.1 is enabled or when Connection header is set keep-alive under HTTP/1.0
		 *
		 * @retval false connection will be closed. This  result is returned when
		 *   HTTP/1.1 is disabled or when Connection header is set to close under  HTTP/1.1
		 *
		 *   By default, server also check input protocol header. In http/1.0 with no
		 *   Connection field in output header function returns false, because server
		 *   doesn't support keep-alive for HTTP/1.0 protocol. But handler can
		 *   supply right headers to keep connection alive. If this emited in the header
		 *   field, function starts returning true and server will not close connection
		 *   after handler returns.
		 *
		 *   When HTTP/1.1 is enabled, chunked transfer encoding is in effect. By default,
		 *   server reports this in the headers and sets keepAlive to true. When there
		 *   is user header Transfer-Encoding, "chunked" is not enabled by default and
		 *   handler must implement own keep-alive mechanism. If handler don't want to use
		 *   keep-alive, it must emit header Connection: close. If request contains
		 *   Connection: close and no Connection: close header is emited, server
		 *   adds own header to ensure client, that connection will be closed
		 */
		virtual bool keepAlive() const = 0;

		///Retrieves underlying connection object
		/**@note writting directly to the connection object can break
		 *  protocol rules and causes that connection will be reset by
		 *  other side. If transfer encoding is set to chunked or
		 *  compressed, direct writting will bypass function providing
		 *  these features!
		 *
		 *  Use this function after you switched protocols
		 *  (for example after sending 101 status code) or if you
		 *  reset connection to http/1.0 to ensure that neither chunked
		 *  nor compressed transfer encoding is in effect.
		 *
		 * @return
		 */
		virtual PNetworkStream getConnection() = 0;


		///sets context for this request
		/**
		 * @param context pointer to context. Context is cleared when request is finished
		 *
		 * @note to optimize context allocation, use getContextAllocator()
		 */
		virtual void setRequestContext(IHttpHandlerContext *context) = 0;
		///sets context for this connection
		/**
		 * @param context pointer to context. Context is destroyed when connection closes. You must be careful while reading context, because
		 * it can belongs to different handler.
		 *
		 * @note to optimize context allocation, use getContextAllocator()
		 */
		virtual void setConnectionContext(IHttpHandlerContext *context) = 0;

		///Retrieves context associated with this request
		virtual IHttpHandlerContext *getRequestContext() const = 0;

		///Retrieves context associated with this connection
		virtual IHttpHandlerContext *getConnectionContext() const = 0;


		///Retrieves size of POST body
		/**
		 * @retval 0 request has no post body.
		 * @retval naturalNull request has post body encoded in chunked transfer encoding,
		 * 	so system cannot determine length of body
		 * @return function returns length of post body in bytes. Note that returned value
		 * represents count of bytes that has not been read from the input stream. For
		 * example if POST request contains 100 bytes and you already read 10 bytes,
		 * function returns value 90. In case, that POST is chunked, function returns
		 * naturalNull until end of stream reached. Then it returns zero
		 */
		virtual natural getPostBodySize() const = 0;


		///Begins IO operation
		/**
		 * Handler can operate in two modes. Busy mode and IO mode. In busy
		 * mode, server expects that handler working on its task and does not
		 * perform any IO which may block handler for some time. Handler can
		 * start IO mode before it starts to read from network or perform any
		 * other blocking possible operation.
		 *
		 * This function enters into IO mode.
		 *
		 * There can be a lot of threads in IO mode, but limited count of threads
		 * in busy mode. This helps to find optimal thread count enough to cover
		 * all IO/network operations without blocking busy threads.
		 */
		virtual void beginIO() = 0;

		///Finish IO mode
		/**
		 * @see beginIO
		 *
		 * @note Function enters into busy mode. Because count of busy threads
		 * is limited, function can block thread execution waiting on busy semaphore.
		 */
		virtual void endIO() = 0;


		///Sets maximum post body size
		/** Function must be called everytime before request is read by the handler
		 * It default value is infinite (naturalNull).
		 *
		 * When specified count of bytes is less than Content-Lenght, function generates
		 * HTTP Status code 413 Request Entity is too large. When chunked post is in the effect,
		 * function starts to track length of the request until it reaches specified count
		 *
		 * Do not call function during reading the data, because it always resets the counter
		 * of already read bytes.
		 *
		 * @param bytes count of bytes to limit the read
		 *
		 * @exception HttpStatusException is thrown, when request is larger. This can be thrown
		 * anytime later when reading meets this limit.
		 */
		virtual void setMaxPostSize(natural bytes) = 0;

		class SectionIO {
		public:
			SectionIO(IHttpRequest &r):r(r) {r.beginIO();}
			~SectionIO() {r.endIO();}
			IHttpRequest &r;
		};
};

	class IHttpHandler {
	public:

		typedef IHttpRequest::HeaderValue HeaderValue;

		enum Status {
		///Request has been processed
		/** this not correct way, how to report status 200, because handler should send headers and generate content. Return value
		 * is ignored, but you use it to say that request has been really processed correctly
		 */
			stOK = 200,
		///Request has been rejected
		/** Server will continue to search next handler for this path. If there is no more handlers, generates 404 page */
			stReject = 0,
		///continue receiving data
		/** This state can be used any-time you want to let server wait incoming data. It doesn't need 100 continue expectation.
		 * If client expects 100 status code, server automatically sends this code when stContinue is returned (this status
		 * is also send automatically when handler starts to read input stream and client includes Expect header field)
		 */
			stContinue = 100,
			stBadRequest = 400,
			stUnauthorized = 401,
			stNotFound = 404,
			stForbidden = 403,
			stNotModified = 304,
			stInternalError = 500,
			stNotImplemented = 501,
			stBadGateway = 502,
			stServiceUnavailble = 503,
			stTimeout = 504,
			stUnsupportedVersion = 505,
			stCreated = 201,
			stAccepted = 202,
			stNonAuthorativeInfo = 203,
			stNoContent = 204,
			stResetContent = 205,
			stGone = 410,
			stExpectationFailed = 417,
			stMethodNotAllowed = 405,
			stNotAcceptable = 406,

			///call handler after data are written
			/**handler can return this state when it is unable to write whole block by single write operation.
			 * This state closes current chunk, sends data to the output and waits for emptying output buffer.
			 * Once the chunk is sent, function onData is called again.
			 *
			 */
			stWaitForWrite = 999
		};



		///Called by HTTP server to process request
		/**
		 * @param request request information
		 * @param vpath virtual path - path relative to root where handler is mapped. To retrieve whole
		 *   path, use IHttpRequest::getPath(). vpath can contain query string
		 * @return Function should return one of Status result, such a stOK. If header has been sent, return value is ignored
		 *  unless stContinue is used.
		 *
		 */
		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath) = 0;

		///Called when unbound data arrived to the connection after request has been processed with stContinue reply
		/**
		 * @param request request descriptor, same object as passed to onRequest. You can
		 *   request and connection context stored with function setRequestContext() or setConnectionContext()
		 * @return same meaning as onRequest. read description on stXXXX status codes.<
		 */
		virtual natural onData(IHttpRequest &request) = 0;


		virtual ~IHttpHandler() {}
		typedef IHttpRequest::SectionIO SectionIO;

	};


	class IHttpMapper: virtual public IInterface {
	public:
		virtual void addSite(ConstStrA path, IHttpHandler *handler) = 0;
		virtual void removeSite(ConstStrA path) = 0;
	};


	///Interface can be extracted from IHttpHandler, if implementation supports this
	class IHttpPeerInfo: virtual public IInterface {
	public:
		///Returns network address of the peer
		virtual const NetworkAddress &getPeerAddr() const = 0;
		virtual ConstStrA getPeerAddrStr() const = 0;

		///Retrieves connection source identifier
		/** This is used when multiport server is opened.
		 *  Master port is port specified during server's start. Other ports are specified through
		 *  addPort function in HttpServer class. Each addPort returns sourceId of this port.
		 *
		 * @retval 0 this connection has been created at master port
		 * @retval 1.. this connection has been create at other ports. First added port has ID 1, second
		 * 	port 2, etc.
		 */
		virtual natural getSourceId() const = 0;

	};
	class IHttpLiveLog: virtual public IInterface {
	public:
		virtual void addLiveLog(ConstStrA path) = 0;
		///Adds livelog interface with autentification
		/**
		 * @param path relative server path
		 * @param realm text shows as server realm
		 * @param userList list of users and passwords.
		 *
		 *  User is defined as username:password
		 *  Multiple users are separated by comma character. This comma cannot be
		 *  part of username and password
		 */
		virtual void addLiveLog(ConstStrA path, ConstStrA realm, ConstStrA userList ) = 0;
		virtual ~IHttpLiveLog() {}
	};
}


#endif /* JSONRPCSERVER_HTTPREQUEST_H_ */
