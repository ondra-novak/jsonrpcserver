/*
 * iclient.h
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_JSONRPC_ICLIENT_H_
#define LIBS_JSONRPCSERVER_JSONRPC_ICLIENT_H_
#include <lightspeed/utils/json/json.h>

#include "../httpclient/httpClient.h"
#include "rpchandler.h"
#include "rpcerror.h"

namespace jsonrpc {

using namespace LightSpeed;

struct ClientConfig: public BredyHttpClient::ClientConfig {
	StringA url;
	JSON::PFactory jsonFactory;
};

class IClient {
public:

	class Result: public JSON::ConstValue {
	public:
		Result(JSON::ConstValue result, JSON::ConstValue context): JSON::ConstValue(result),context(context) {}
		JSON::ConstValue context;
	};
	virtual ~IClient() {}

};

using jsonsrv::RpcError;



}




#endif /* ICLIENT_H_ */
