/*
 * interfaces.h
 *
 *  Created on: 2. 5. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_HTTPCLIENT_INTERFACES_H_
#define LIBS_JSONRPCSERVER_HTTPCLIENT_INTERFACES_H_
#include "lightspeed/base/streams/netio_ifc.h"


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
		StringA proxyAddr;
		///Content of autorization header. If empty, no autorzation header is emitted
		StringA authorization;
		///true, if proxy is defined, otherwise false (client should connect directly)
		bool defined;

		Result(ConstStrA proxy):proxyAddr(proxy),defined(true) {}
		Result(ConstStrA proxy, ConstStrA authorization):proxyAddr(proxy),authorization(authorization),defined(true) {}
		Result():defined(false) {}
	};
	virtual Result  getProxy(ConstStrA hostname) = 0;
};

}



#endif /* LIBS_JSONRPCSERVER_HTTPCLIENT_INTERFACES_H_ */
