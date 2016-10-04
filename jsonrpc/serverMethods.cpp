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

JSON::ConstValue ServerMethods::rpcListMethods(const Request& r) {
	WeakRefPtr<IDispatcher> disp(r.dispatcher);
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


FResponse ServerMethods::rpcMulticall(const Request& r) {
//multicall forms
//["method",[params],[params],[params],/...] => result:{results:[...],errors:[...]}
//[["method",[params]],["method",[params],...] => result:{results:[...],errors:[...]}

	if (r.params.empty()) throw MethodException(THISLOCATION,400,"Invalid parameters");
	if (r.params[0]->isString()) {
		if (r.params.length() < 2)
			throw MethodException(THISLOCATION,400,"Need at least two parameters");
		return runMulticall1(r);
	}  else if (r.params[0]->isArray()) {
		return runMulticallN(r);
	} else {
		throw MethodException(THISLOCATION,400,"Need at least two parameters");
	}

}

class MakeStatResult {
public:
	StringA name;
	JSON::Builder json;

	MakeStatResult( const JSON::Builder &json, const StringA &name)
		:name(name),json(json) {}
	JSON::ConstValue operator()(const Response &resp) const {
		return json(name,resp.result);
	}
};

class CombineStats{
public:
	JSON::Container result;
	CombineStats(const JSON::Container &result):result(result) {}
	Response operator()(const VariantPair &vp) {
		const VariantPair *ptr = &vp, *newptr;
		result.load(ptr->second.getValue<JSON::ConstValue>());
		newptr = ptr->first.getPtr<VariantPair>();
		while (newptr) {
			ptr = newptr;
			result.load(ptr->second.getValue<JSON::ConstValue>());
			newptr = ptr->first.getPtr<VariantPair>();
		}
		result.load(ptr->first.getValue<JSON::ConstValue>());
		return Response(result);
	}
};

FResponse ServerMethods::rpcStats(const Request& r) {
	typedef Future<JSON::ConstValue> StatResult;
	typedef AutoArray<StatResult,SmallAlloc<256> > Collector;
	Collector responses;

	class CollectStats: public IMethodRegister::IMethodEnum {
	public:
		virtual void operator()(ConstStrA prototype) const {

			Dispatcher::PMethodHandler h = dispatch.findMethod(prototype);
			if (h == null) return;
			try {
				StatResult resp = StatResult::transform((*h)(r),MakeStatResult(r.json, prototype));
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
	stats.enumMethods(c);

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
	return FResponse::transform(combined, CombineStats(r.json.object()));


}

Void ServerMethods::rpcCrash(const Request& ) {
	int *x = (int *)1;
	*x = 2; //crash
	return Void();
}


Void ServerMethods::rpcCrashScheduler(const Request& r) {
	WeakRefPtr<BredyHttpSrv::IHttpContextControl> ptr(r.httpRequest);
	BredyHttpSrv::IJobScheduler *sch = ptr->getIfcPtr<BredyHttpSrv::IJobScheduler>();
	sch->schedule(Action::create(this,&ServerMethods::rpcCrash,r),3);
	return Void();
}

JSON::ConstValue ServerMethods::rpcHelp(const Request& r) {
}

JSON::ConstValue ServerMethods::rpcPing(const Request& r) {
	return r.params;
}

IRpcNotify *IRpcNotify::fromRequest(const Request &r) {
	return r.dispatcher.lock()->getIfcPtr<IRpcNotify>();
}

Void ServerMethods::rpcPingNotify(const Request& r) {
	IRpcNotify *ntf = IRpcNotify::fromRequest(r);
	if (ntf == 0) throw MethodException(THISLOCATION,400,"Not available for this connection");
	ntf->sendNotification(r.methodName.getStringA(),r.params,IRpcNotify::standardTimeout);
	return Void();
}


static void delayedResponse(Promise<JSON::ConstValue> resp) {
	resp.resolve(JSON::getConstant(JSON::constTrue));
}


Future<JSON::ConstValue> ServerMethods::rpcDelay(const Request& r) {


	Future<JSON::ConstValue> v;
	WeakRefPtr<BredyHttpSrv::IHttpContextControl> ptr(r.httpRequest);
	if (ptr == null) throw CanceledException(THISLOCATION);
	BredyHttpSrv::IJobScheduler &sch = ptr->getIfc<BredyHttpSrv::IJobScheduler>();
	natural secs = r.params[0].getUInt();
	sch.schedule(ThreadFunction::create(&delayedResponse, v.getPromise()),secs);
	return v;
}

//cycles through params of multicall
class MulticallCyclerBase: public FResponse::IObserver {
public:

	struct State {
		JSON::Container results;
		JSON::Container errors;
		JSON::Container logOutput;
		JSON::Container context;
		JSON::Container newContext;
	};

	MulticallCyclerBase(const JSON::Builder &json, State &state, const JSON::ConstValue &initContext = null)
		:state(state),json(json)
	{
		if (initContext) {
			state.errors = json.array();
			state.results = json.array();
			state.context = json(initContext);
			state.newContext = json.array();
			state.logOutput = json.array();
		}
	}

	virtual void resolve(const Response &resp) throw() {
		//add to results
		state.results.add(resp.result);
		//merge contexts
		if (resp.context != null) {
			state.context.load(resp.context);
			state.newContext.load(resp.context);
		}
		if (resp.logOutput != null)
			state.logOutput.add(resp.logOutput);
		//advence to next argument
		goon();

	}
	virtual void resolve(const PException &e) throw() {

		JSON::ConstValue exceptionObj;
		RpcException *rpce = dynamic_cast<RpcException *>(e.get());
		if (rpce) {
			exceptionObj = rpce->getJSON(json);
		} else {
			exceptionObj = UncauchException(THISLOCATION,*e).getJSON(json);
		}
		state.errors.add(exceptionObj);
		state.logOutput.add(exceptionObj);
		state.results.add(JSON::getConstant(JSON::constNull));

		goon();

	}

	virtual void goon() {}



protected:
	State &state;

	JSON::Builder json;

};


class MulticallCycler1: public MulticallCyclerBase {
	State state;
public:
	MulticallCycler1(const Request &r, const Promise<Response> &retval)
		:MulticallCyclerBase(r.json,state,r.context)
		,retval(retval)
		,r(r) {}

	void goon() {

		natural cnt = r.params.length()-1;
		natural pos = state.results.length();
		WeakRefPtr<IDispatcher> dispatcher(r.dispatcher);
		//dispatcher is no longer available, reject rest of multicall
		if (dispatcher == null) {

			CanceledException canceled(THISLOCATION);canceled.setStaticObj();
			UncauchException uncouch(THISLOCATION,canceled);
			JSON::ConstValue excp = uncouch.getJSON(r.json);


			while (state.results.length() < cnt) {
				state.results.add(JSON::getConstant(JSON::constNull));
				state.errors.add(excp);
			}
		}


		while (pos < cnt) {

			Request req = r;
			req.methodName= r.params[0];
			req.params = r.params[pos+1];
			req.context = state.context;
			FResponse resp = dispatcher->callMethod(req);

			MulticallCyclerBase directObserver(r.json,state);

			if (!resp.tryObserver(&directObserver)) {
				resp.addObserver(this);
				return;
			}
			pos = state.results.length();
		}

		retval.resolve(Response(r.json("results",state.results)
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
		:MulticallCyclerBase(r.json,state,r.context)
		,retval(retval)
		,r(r) {}

	void goon() {

		natural cnt = r.params.length();
		natural pos = state.results.length();
		WeakRefPtr<IDispatcher> dispatcher(r.dispatcher);
		//dispatcher is no longer available, reject rest of multicall
		if (dispatcher == null) {

			CanceledException canceled(THISLOCATION);canceled.setStaticObj();
			UncauchException uncouch(THISLOCATION,canceled);
			JSON::ConstValue excp = uncouch.getJSON(r.json);


			while (state.results.length() < cnt) {
				state.results.add(JSON::getConstant(JSON::constNull));
				state.errors.add(excp);
			}
		}


		while (pos < cnt) {

			Request req = r;
			req.methodName= r.params[pos][0];
			req.params = r.params[pos][1];
			req.context = state.context;
			FResponse resp = dispatcher->callMethod(req);

			MulticallCyclerBase directObserver(r.json,state);

			if (!resp.tryObserver(&directObserver)) {
				resp.addObserver(this);
				return;
			}
			pos = state.results.length();
		}

		retval.resolve(Response(r.json("results",state.results)
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
