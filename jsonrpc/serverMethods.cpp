/*
 * serverMethods.cpp
 *
 *  Created on: 12. 8. 2016
 *      Author: ondra
 */

#include "serverMethods.h"

#include "../httpserver/IJobScheduler.h"
#include "lightspeed/base/memory/weakref.h"
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
	return r.params;
}

JSON::ConstValue ServerMethods::rpcPingNotify(const Request& r) {

}

class DelayedResponse:public BredyHttpSrv::IHttpHandlerContext {
public:

	DelayedResponse(const Promise<JSON::ConstValue> &resp):meweak(this),resp(resp) {}
	~DelayedResponse() {
		meweak.setNull();
	}

	static void delayedResponse(const WeakRef<DelayedResponse> &ref) {
		WeakRefPtr<DelayedResponse> ptr(ref);
		if (ptr != null) ptr->sendReply();
	}

	void sendReply() {
		resp.resolve(JSON::getConstant(JSON::constTrue));
	}

	WeakRefTarget<DelayedResponse> meweak;
	Promise<JSON::ConstValue> resp;
};


Future<JSON::ConstValue> ServerMethods::rpcDelay(const Request& r) {


	Future<JSON::ConstValue> v;
	DelayedResponse *dr = new DelayedResponse(v.getPromise());
	r.httpRequest->setRequestContext(dr);
	WeakRef<DelayedResponse> w = dr->meweak;

	BredyHttpSrv::IJobScheduler &sch = r.httpRequest->getIfc<BredyHttpSrv::IJobScheduler>();
	natural secs = r.params[0].getUInt();
	sch.schedule(ThreadFunction::create(&DelayedResponse::delayedResponse, w),secs);
	return v;
}

} /* namespace jsonrpc */
