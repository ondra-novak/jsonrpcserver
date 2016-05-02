/*
 * simpleHttpsProvider.h
 *
 *  Created on: 2. 5. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_HTTPCLIENT_SIMPLEHTTPSPROVIDER_H_
#define LIBS_JSONRPCSERVER_HTTPCLIENT_SIMPLEHTTPSPROVIDER_H_

namespace BredyHttpClient {

using namespace LightSpeed;

#include "interfaces.h"


class SimpleHttps: public IHttpsProvider {
public:
	virtual PNetworkStream connectTLS(PNetworkStream connection, ConstStrA hostname);

	static SimpleHttps *getInstance();


};


}



#endif /* LIBS_JSONRPCSERVER_HTTPCLIENT_SIMPLEHTTPSPROVIDER_H_ */
