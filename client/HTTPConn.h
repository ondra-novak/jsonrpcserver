/*
 * HTTPConn.h
 *
 *  Created on: 19. 10. 2015
 *      Author: ondra
 */

#ifndef JSONRPC_BREDY_CLIENT_HTTPCONN_H_
#define JSONRPC_BREDY_CLIENT_HTTPCONN_H_


#include "HTTPConn.h"

#include <lightspeed/base/exceptions/exception.h>
#include <lightspeed/base/exceptions/ioexception.h>
#include "lightspeed/base/streams/netio.h"
#include "../httpserver/headerValue.h"
#include "lightspeed/base/containers/stringpool.tcc"

#include "lightspeed/base/containers/map.h"
namespace jsonrpc {

using BredyHttpSrv::HeaderValue;

using namespace LightSpeed;

class HTTPConn: public INetworkStream {
public:

	HTTPConn(PNetworkStream stream);


	void openRequest(ConstStrA uri, ConstStrA method=ConstStrA());
	void setHeader(ConstStrA field, ConstStrA value);
	void sendRequest();
	void sendRequest(ConstStrA data);
	void sendRequestStreamBody();

	HeaderValue getHeader(ConstStrA field);
	natural getStatus() const;
	ConstStrA getStatusMsg() const;



	//overrides
	virtual natural read(void *buffer,  natural size);
	virtual natural peek(void *buffer, natural size) const ;
	virtual bool canRead() const ;
	virtual natural dataReady() const ;
	virtual natural write(const void *buffer,  natural size) ;
	virtual bool canWrite() const ;
	virtual void flush() ;
	virtual void closeOutput();
	virtual natural getDefaultWait() const;
	virtual void setWaitHandler(WaitHandler *handler);
	virtual WaitHandler *getWaitHandler() ;
	virtual void setTimeout(natural time_in_ms) ;
	virtual natural getTimeout() const ;
	virtual natural wait(natural waitFor, natural timeout) const ;
	virtual natural doWait(natural waitFor, natural timeout) const ;

public:

	PNetworkStream stream;
	PNetworkStream proxy;
	PNetworkStream nullStream;
	StringPoolA hdrfields;
	typedef Map<StringPoolA::Str,StringPoolA::Str> HeaderMap;
	HeaderMap requestHdr, responseHdr;
	natural status;
	StringPoolA::Str statusMsg,method,uri;

	void parseResponse();

private:
	void sendHeaders(PrintTextA  &out);
};

class HTTPReadException: public IOException {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	HTTPReadException(const ProgramLocation &loc, natural code, StringA msg)
		:Exception(loc),code(code),msg(msg) {}
	~HTTPReadException() throw() {}

	natural getCode() const {
		return code;
	}

	ConstStrA getMsg() const {
		return msg;
	}


protected:
	natural code;
	StringA msg;

	virtual void message(ExceptionMsg &msg) const ;


};

} /* namespace jsonrpc */

#endif /* JSONRPC_BREDY_CLIENT_HTTPCONN_H_ */
