/*
 * serverMethods.cpp
 *
 *  Created on: 12. 8. 2016
 *      Author: ondra
 */

#include "serverMethods.h"

#include "../httpserver/IJobScheduler.h"
#include "lightspeed/base/memory/weakref.h"

#include "errors.h"
#include "lightspeed/base/interface.tcc"

#include "ipeer.h"
#include "methodreg.h"
#include "methodreg.tcc"
#include "rpcnotify.h"
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

JValue ServerMethods::rpcListMethods(const Request& r) {
	WeakRefPtr<IDispatcher> disp(r.dispatcher);
	IMethodRegister &reg = disp->getIfc<IMethodRegister>();

	class Enum: public IMethodRegister::IMethodEnum {
	public:
		JArray &result;
		mutable ConstStrA prevPrototype;
		natural limitVersion;
		Enum(JArray &result):result(result) {}
		virtual void operator()(StrView prototype) const {
			if (prototype == StrView(prevPrototype)) return;
			prevPrototype = ConstStrA(prototype);
			result.add(JValue(prototype));
		}

	};
	JArray result;
	reg.enumMethods(Enum(result),r.version);
	return result;
}


FResponse ServerMethods::rpcMulticall(const Request& r) {
//multicall forms
//["method",[params],[params],[params],/...] => result:{results:[...],errors:[...]}
//[["method",[params]],["method",[params],...] => result:{results:[...],errors:[...]}

	if (r.params.empty()) throw MethodException(THISLOCATION,400,"Invalid parameters");
	if (r.params[0].type() == json::string) {
		if (r.params.size() < 2)
			throw MethodException(THISLOCATION,400,"Need at least two parameters");
		return runMulticall1(r);
	}  else if (r.params[0].type() == json::array) {
		return runMulticallN(r);
	} else {
		throw MethodException(THISLOCATION,400,"Need at least two parameters");
	}

}

class MakeStatResult {
public:
	StringA name;

	MakeStatResult( const StringA &name)
		:name(name) {}
	JValue operator()(const Response &resp) const {
		return JObject(convStr(name),resp.result);
	}
};

class CombineStats{
public:
	JObject result;
	CombineStats(const JValue &result):result(result) {}
	Response operator()(const VariantPair &vp) {
		const VariantPair *ptr = &vp, *newptr;
		result.merge(ptr->second.getValue<JValue>());
		newptr = ptr->first.getPtr<VariantPair>();
		while (newptr) {
			ptr = newptr;
			result.merge(ptr->second.getValue<JValue>());
			newptr = ptr->first.getPtr<VariantPair>();
		}
		result.merge(ptr->first.getValue<JValue>());
		return Response(result);
	}
};

FResponse ServerMethods::rpcStats(const Request& r) {
	typedef Future<JValue> StatResult;
	typedef AutoArray<StatResult,SmallAlloc<256> > Collector;
	Collector responses;

	class CollectStats: public IMethodRegister::IMethodEnum {
	public:
		virtual void operator()(StrView prototype) const {

			Dispatcher::PMethodHandler h = dispatch.findMethod(prototype);
			if (h == null) return;
			try {
				StatResult resp = StatResult::transform((*h)(r),MakeStatResult(prototype));
				responses.add(resp);
			} catch (...) {
				StatResult r;
				responses.add(r);
				r.getPromise().rejectInCatch();
			}
		}
		CollectStats(const Request& r, Collector &responses, Dispatcher &dispatch)
				:r(r),responses(responses),dispatch(dispatch) {}
	protected:
		const Request& r;
		Collector &responses;
		Dispatcher &dispatch;
	};

	CollectStats c(r,responses,stats);
	stats.enumMethods(c,r.version);

	if (responses.empty()) {
		throw MethodException(THISLOCATION,204,"No statistics are available at the moment");
	}
	if (responses.length() == 1) {
		return FResponse::transform(responses[0]);
	}
	Future<VariantPair> combined = responses[0] && responses[1];
	for (natural i = 2; i < responses.length(); i++) {
		combined = combined && responses(2);
	}
	return FResponse::transform(combined, CombineStats(JValue(json::object)));


}

Void ServerMethods::rpcCrash(const Request& ) {
	int *x = (int *)1;
	*x = 2; //crash
	return Void();
}


Void ServerMethods::rpcCrashScheduler(const Request& r) {
	WeakRefPtr<IPeer> peer(r.peer);
	if (peer == null) throw CanceledException(THISLOCATION);
	BredyHttpSrv::IJobScheduler *sch = peer->getHttpRequest()->getIfcPtr<BredyHttpSrv::IJobScheduler>();
	sch->schedule(Action::create(this,&ServerMethods::rpcCrash,r),3);
	return Void();
}

JValue ServerMethods::rpcHelp(const Request& r) {
}

JValue ServerMethods::rpcPing(const Request& r) {
	return r.params;
}

IRpcNotify *IRpcNotify::fromRequest(const Request &r) {
	return r.dispatcher.lock()->getIfcPtr<IRpcNotify>();
}

Void ServerMethods::rpcPingNotify(const Request& r) {
	IRpcNotify *ntf = IRpcNotify::fromRequest(r);
	if (ntf == 0) throw MethodException(THISLOCATION,400,"Not available for this connection");
	ntf->sendNotification(r.methodName.getString(),r.params,IRpcNotify::standardTimeout);
	return Void();
}


static void delayedResponse(Promise<JValue> resp) {
	resp.resolve(JValue(true));
}


Future<JValue> ServerMethods::rpcDelay(const Request& r) {

	Future<JValue> v;
	WeakRefPtr<IPeer> peer(r.peer);
	if (peer == null) throw CanceledException(THISLOCATION);
	BredyHttpSrv::IHttpRequestInfo *httpreq = peer->getHttpRequest();
	BredyHttpSrv::IJobScheduler &sch = httpreq->getIfc<BredyHttpSrv::IJobScheduler>();
	natural secs = r.params[0].getUInt();
	sch.schedule(ThreadFunction::create(&delayedResponse, v.getPromise()),secs);
	return v;
}

//cycles through params of multicall
class MulticallCyclerBase: public FResponse::IObserver {
public:

	struct State {
		JArray results;
		JArray errors;
		JArray logOutput;
		JObject context;
		JObject newContext;
	};

	MulticallCyclerBase(State &state, const JValue &initContext = JValue())
		:state(state)
	{
		if (initContext.defined()) {
			state.context.setBaseObject(initContext);
		}
	}

	virtual void resolve(const Response &resp) throw() {
		//add to results
		state.results.add(resp.result);
		//merge contexts
		if (resp.context.defined()) {
			state.context.merge(resp.context);
			state.newContext.merge(resp.context);
		}
		if (resp.logOutput.defined())
			state.logOutput.add(resp.logOutput);
		//advence to next argument
		goon();

	}
	virtual void resolve(const PException &e) throw() {

		JValue exceptionObj;
		RpcException *rpce = dynamic_cast<RpcException *>(e.get());
		if (rpce) {
			exceptionObj = rpce->getJSON();
		} else {
			exceptionObj = UncauchException(THISLOCATION,*e).getJSON();
		}
		state.errors.add(exceptionObj);
		state.logOutput.add(exceptionObj);
		state.results.add(JValue(nullptr));

		goon();

	}

	virtual void goon() {}



protected:
	State &state;


};


class MulticallCycler1: public MulticallCyclerBase {
	State state;
public:
	MulticallCycler1(const Request &r, const Promise<Response> &retval)
		:MulticallCyclerBase(state,r.context)
		,retval(retval)
		,r(r) {}

	void goon() {

		natural cnt = r.params.size()-1;
		natural pos = state.results.size();
		WeakRefPtr<IDispatcher> dispatcher(r.dispatcher);
		//dispatcher is no longer available, reject rest of multicall
		if (dispatcher == null) {

			CanceledException canceled(THISLOCATION);canceled.setStaticObj();
			UncauchException uncouch(THISLOCATION,canceled);
			JValue excp = uncouch.getJSON();


			while (state.results.size() < cnt) {
				state.results.add(JValue(nullptr));
				state.errors.add(excp);
			}
		}


		while (pos < cnt) {

			Request req = r;
			req.methodName= r.params[0];
			req.params = r.params[pos+1];
			req.context = state.context;
			FResponse resp = dispatcher->callMethod(req);

			MulticallCyclerBase directObserver(state);

			if (!resp.tryObserver(&directObserver)) {
				resp.addObserver(this);
				return;
			}
			pos = state.results.size();
		}

		retval.resolve(Response(JObject("results",state.results)
									  ("errors",state.errors),state.newContext,state.logOutput));
		delete this;
	}
protected:
	//promise will be resolved once everything is done
	Promise<Response> retval;
	//contains request - we can store reference because it is valid during method is being executed
	const Request &r;

};

class MulticallCyclerN: public MulticallCyclerBase {
	State state;
public:
	MulticallCyclerN(const Request &r, const Promise<Response> &retval)
		:MulticallCyclerBase(state,r.context)
		,retval(retval)
		,r(r) {}

	void goon() {

		natural cnt = r.params.size();
		natural pos = state.results.size();
		WeakRefPtr<IDispatcher> dispatcher(r.dispatcher);
		//dispatcher is no longer available, reject rest of multicall
		if (dispatcher == null) {

			CanceledException canceled(THISLOCATION);canceled.setStaticObj();
			UncauchException uncouch(THISLOCATION,canceled);
			JValue excp = uncouch.getJSON();


			while (state.results.size() < cnt) {
				state.results.add(JValue(nullptr));
				state.errors.add(excp);
			}
		}


		while (pos < cnt) {

			Request req = r;
			req.methodName= r.params[pos][0];
			req.params = r.params[pos][1];
			req.context = state.context;
			FResponse resp = dispatcher->callMethod(req);

			MulticallCyclerBase directObserver(state);

			if (!resp.tryObserver(&directObserver)) {
				resp.addObserver(this);
				return;
			}
			pos = state.results.size();
		}

		retval.resolve(Response(JObject("results",state.results)
									  ("errors",state.errors),state.newContext,state.logOutput));
		delete this;
	}
protected:
	//promise will be resolved once everything is done
	Promise<Response> retval;
	//contains request - we can store reference because it is valid during method is being executed
	const Request &r;

};



FResponse ServerMethods::runMulticall1(const Request& r) {


	FResponse resp;
	MulticallCycler1 *cycler = new MulticallCycler1(r,resp.getPromise());
	cycler->goon();
	return resp;

}

FResponse ServerMethods::runMulticallN(const Request& r) {

	FResponse resp;
	MulticallCyclerN *cycler = new MulticallCyclerN(r,resp.getPromise());
	cycler->goon();
	return resp;

}

} /* namespace jsonrpc */
