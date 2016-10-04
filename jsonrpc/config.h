/*
 * config.h
 *
 *  Created on: 3. 10. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_JSONRPC_ICLIENT_CONFIG_H_
#define LIBS_JSONRPCSERVER_JSONRPC_ICLIENT_CONFIG_H_

namespace jsonrpc {

using namespace LightSpeed;

struct ClientConfig: public BredyHttpClient::ClientConfig {
	StringA url;
	JSON::PFactory jsonFactory;
};

}



#endif /* CONFIG_H_ */
