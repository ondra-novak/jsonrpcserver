/*
 * serverMethods.cpp
 *
 *  Created on: 12. 8. 2016
 *      Author: ondra
 */

#include "serverMethods.h"

#include "methodreg.h"
#include "methodreg.tcc"
namespace jsonrpc {

void ServerMethods::registerServerMethods(IMethodRegister &reg, natural flags) {
	if (flags & flagEnableListMethods) {
		reg.regMethod("Server.listMethods:", this, &ServerMethods::rpcListMethods);
	}
	if (flags & flagEnableMulticall) {
		reg.regMethod("Server.multicall", this, &ServerMethods::rpcMulticall);
	}
	if (flags & flagEnableStatHandler) {
		reg.regMethod("Server.stats", this, &ServerMethods::rpcStats);
	}
	if (flags & flagDevelopMode) {
		reg.regMethod("Server.crash", this, &ServerMethods::rpcCrash);
		reg.regMethod("Server.crashScheduler",this,&ServerMethods::rpcCrashScheduler);
		reg.regMethod("Server.help:s",this,&ServerMethods::rpcHelp);
		reg.regMethod("Server.ping",this,&ServerMethods::rpcPing);
		reg.regMethod("Server.pingNotify",this,&ServerMethods::rpcPingNotify);
		reg.regMethod("Server.delay:n",this,&ServerMethods::rpcDelay);
	}

}

JSON::ConstValue ServerMethods::rpcListMethods(const Request& r) {
	IDispatcher *disp = r.dispatcher;
	IMethodRegister &reg = disp->getIfc<IMethodRegister>();

	class Enum: public IMethodRegister::IMethodEnum {
	public:
		mutable JSON::Builder::Array result;
		mutable ConstStrA prevPrototype;
		natural limitVersion;
		Enum(JSON::Builder::Array result):result(result) {}
		virtual void operator()(ConstStrA prototype) const {
			if (prototype == prevPrototype) return;
			prevPrototype = prototype;
			result << prototype;
		}

	};
	JSON::Builder::Array result = r.json.array();
	reg.enumMethods(Enum(result));
	return result;
}


JSON::ConstValue ServerMethods::rpcMulticall(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcStats(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcCrash(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcCrashScheduler(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcHelp(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcPing(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcPingNotify(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcDelay(const Request& r) {
}

} /* namespace jsonrpc */
