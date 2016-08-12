/*
 * server.h
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_165465416_SERVER_H_
#define JSONRPC_JSONRPCSERVER_165465416_SERVER_H_

#include "../httpserver/httprequest.h"
#include "dispatch.h"
#include "httpHandler.h"
#include "serverMethods.h"

namespace jsonrpc {

class Server: public ServerMethods, public Dispatcher, public HttpHandler {
public:
	Server();
};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONRPCSERVER_165465416_SERVER_H_ */
