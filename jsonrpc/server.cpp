/*
 * server.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "server.h"

namespace jsonrpc {

Server::Server():HttpHandler(static_cast<IDispatcher &>(*this)) {
	// TODO Auto-generated constructor stub

}

} /* namespace jsonrpc */
