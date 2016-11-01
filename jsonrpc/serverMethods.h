/*
 * serverMethods.h
 *
 *  Created on: 12. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_131658031_SERVERMETHODS_H_
#define JSONRPC_JSONRPCSERVER_131658031_SERVERMETHODS_H_
#include <lightspeed/base/types.h>

#include "dispatch.h"

namespace jsonrpc {

using namespace LightSpeed;

class IMethodRegister;

class ServerMethods {
public:

	static const natural flagDevelopMode = 1;
	static const natural flagEnableMulticall = 2;
	static const natural flagEnableListMethods = 4;
	static const natural flagEnableStatHandler = 8;

	void registerServerMethods(IMethodRegister &reg, natural flags);



	JValue rpcListMethods(const Request &r);
	FResponse rpcMulticall(const Request &r);
	FResponse rpcStats(const Request &r);
	Void rpcCrash(const Request &r);
	Void rpcCrashScheduler(const Request &r);
	JValue rpcHelp(const Request &r);
	JValue rpcPing(const Request &r);
	Void rpcPingNotify(const Request &r);
	Future<JValue> rpcDelay(const Request &r);

	///by registering methods here, one can specify which statistics will be shown in Server.stats()
	Dispatcher stats;

private:

	FResponse runMulticall1(const Request &r);
	FResponse runMulticallN(const Request &r);
};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONRPCSERVER_131658031_SERVERMETHODS_H_ */
