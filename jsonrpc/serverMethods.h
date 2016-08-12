/*
 * serverMethods.h
 *
 *  Created on: 12. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_131658031_SERVERMETHODS_H_
#define JSONRPC_JSONRPCSERVER_131658031_SERVERMETHODS_H_
#include <lightspeed/base/types.h>

#include "idispatch.h"

namespace jsonrpc {

using namespace LightSpeed;

class ServerMethods {
public:

	static const natural flagDevelopMode = 1;
	static const natural flagEnableMulticall = 2;
	static const natural flagEnableListMethods = 4;
	static const natural flagEnableStatHandler = 8;

	void registerServerMethods(IMethodRegister &reg, natural flags);


	JSON::ConstValue rpcListMethods(const Request &r);
	JSON::ConstValue rpcListMethodsDetailed(const Request &r);
	JSON::ConstValue rpcMulticall(const Request &r);
	JSON::ConstValue rpcStats(const Request &r);
	JSON::ConstValue rpcCrash(const Request &r);
	JSON::ConstValue rpcCrashScheduler(const Request &r);
	JSON::ConstValue rpcHelp(const Request &r);
	JSON::ConstValue rpcPing(const Request &r);
	JSON::ConstValue rpcPingNotify(const Request &r);
	JSON::ConstValue rpcDelay(const Request &r);


};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONRPCSERVER_131658031_SERVERMETHODS_H_ */
