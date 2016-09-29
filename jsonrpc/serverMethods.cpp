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

JSON::ConstValue ServerMethods::rpcStats(const Request& r) {
}

Void ServerMethods::rpcCrash(const Request& r) {
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

	MulticallCyclerBase(const JSON::Builder &json, const JSON::ConstValue &ctx)
		:json(json)
	{
		errors = json.array();
		results = json.array();
		context = json(ctx);
		newContext = json.array();
		logOutput = json.array();
	}

	MulticallCyclerBase(
			const JSON::Builder &json,
			const JSON::Container &results,
			const JSON::Container &errors,
			const JSON::Container &logOutput,
			const JSON::Container &context,
			const JSON::Container &newContext)
		:errors(errors)
		,results(results)
		,context(context)
		,newContext(newContext)
		,logOutput(logOutput)
		,json(json) {}


	virtual void resolve(const Response &resp) throw() {
		//add to results
		results.add(resp.result);
		//merge contexts
		if (resp.context != null) {
			context.load(resp.context);
			newContext.load(resp.context);
		}
		if (resp.logOutput != null)
			logOutput.add(resp.logOutput);
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
		errors.add(exceptionObj);
		logOutput.add(exceptionObj);
		results.add(JSON::getConstant(JSON::constNull));

		goon();

	}

	virtual void goon() {}



protected:
	//collector of errors
	JSON::Container errors;
	//collector of results
	JSON::Container results;
	//tracks current context
	JSON::Container context;
	//contains context changes with is sent to response
	JSON::Container newContext;
	//contains items to log out
	JSON::Container logOutput;

	JSON::Builder json;

};


class MulticallCycler1: public MulticallCyclerBase {
public:
	MulticallCycler1(const Request &r, const Promise<Response> &retval)
		:MulticallCyclerBase(r.json,r.context)
		,retval(retval)
		,r(r) {}

	void goon() {

		natural cnt = r.params.length()-1;
		natural pos = results.length();
		WeakRefPtr<IDispatcher> dispatcher(r.dispatcher);
		//dispatcher is no longer available, reject rest of multicall
		if (dispatcher == null) {

			CanceledException canceled(THISLOCATION);canceled.setStaticObj();
			UncauchException uncouch(THISLOCATION,canceled);
			JSON::ConstValue excp = uncouch.getJSON(r.json);


			while (results.length() < cnt) {
				results.add(JSON::getConstant(JSON::constNull));
				errors.add(excp);
			}
		}


		while (pos < cnt) {

			Request req = r;
			req.methodName= r.params[0];
			req.params = r.params[pos+1];
			req.context = context;
			FResponse resp = dispatcher->callMethod(req);

			MulticallCyclerBase directObserver(r.json,results,errors,logOutput,context,newContext);

			if (!resp.tryObserver(&directObserver)) {
				resp.addObserver(this);
				return;
			}
			pos = results.length();
		}

		retval.resolve(Response(r.json("results",results)
									  ("errors",errors),newContext,logOutput));
		delete this;
	}
protected:
	//promise will be resolved once everything is done
	Promise<Response> retval;
	//contains request - we can store reference because it is valid during method is being executed
	const Request &r;

};

class MulticallCyclerN: public MulticallCyclerBase {
public:
	MulticallCyclerN(const Request &r, const Promise<Response> &retval)
		:MulticallCyclerBase(r.json,r.context)
		,retval(retval)
		,r(r) {}

	void goon() {

		natural cnt = r.params.length();
		natural pos = results.length();
		WeakRefPtr<IDispatcher> dispatcher(r.dispatcher);
		//dispatcher is no longer available, reject rest of multicall
		if (dispatcher == null) {

			CanceledException canceled(THISLOCATION);canceled.setStaticObj();
			UncauchException uncouch(THISLOCATION,canceled);
			JSON::ConstValue excp = uncouch.getJSON(r.json);


			while (results.length() < cnt) {
				results.add(JSON::getConstant(JSON::constNull));
				errors.add(excp);
			}
		}


		while (pos < cnt) {

			Request req = r;
			req.methodName= r.params[pos][0];
			req.params = r.params[pos][1];
			req.context = context;
			FResponse resp = dispatcher->callMethod(req);

			MulticallCyclerBase directObserver(r.json,results,errors,logOutput,context,newContext);

			if (!resp.tryObserver(&directObserver)) {
				resp.addObserver(this);
				return;
			}
			pos = results.length();
		}

		retval.resolve(Response(r.json("results",results)
									  ("errors",errors),newContext,logOutput));
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
