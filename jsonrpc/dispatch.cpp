/*
 * dispatch.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "dispatch.h"

#include "errors.h"

#include "lightspeed/base/containers/map.tcc"

#include "lightspeed/base/actions/promise.tcc"
#include "methodreg.tcc"
namespace jsonrpc {

using namespace LightSpeed;

Dispatcher::Dispatcher() {}

template<typename Container>
inline void Dispatcher::createPrototype(ConstStrA methodName, JSON::ConstValue params, Container& container) {

	container.append(methodName);
	container.add(':');
	for (natural i = 0, cnt = params.length();i < cnt; i++) {
		JSON::ConstValue v = params[i];
		char c;
		switch (v->getType()) {
			case JSON::ndArray: c = 'a';break;
			case JSON::ndBool: c = 'b';break;
			case JSON::ndObject: c= 'c';break;
			case JSON::ndFloat:
			case JSON::ndInt: c = 'n';break;
			case JSON::ndNull: c= 'x';break;
			case JSON::ndString: c = 's';break;
			default: c = 'u';break;
		}
		container.add(c);
	}
}

void logToRpcLog(const Response &r, const Request &req) {

	Dispatcher *d = static_cast<Dispatcher *>(req.dispatcher.get());
	ILog *l = d->getLogObject();
	if (l) {
		l->logMethod(*req.httpRequest,req.methodName,req.params,req.context,r.logOutput);
	}
}


void Dispatcher::callMethod(const Request& req, Promise<Response> result) throw() {

	AutoArray<char, SmallAlloc<256> > prototype;

	createPrototype(req.methodName,req.params,prototype);
	PMethodHandler m1 = findMethod(prototype);
	if (m1 == null) {
		m1 = findMethod(req.methodName);
		if (m1 == null) {
			throw LookupException(THISLOCATION,prototype);
		}
	}


	ILog *l = getLogObject();
	if (l) {
		Future<Response> newresult;
		(*m1)(req, newresult.getPromise());
		newresult.thenCall(Message<void,Response>::create(&logToRpcLog,req));
		result.resolve(newresult);
	} else {
		(*m1)(req, result);
	}

}

Dispatcher::PMethodHandler Dispatcher::findMethod(ConstStrA prototype) {
	Synchronized<RWLock::ReadLock> _(mapLock);

	const  PMethodHandler *h = methodMap.find(StrKey(prototype));
	if (h == 0) return null;
	else {
		return *h;
	}

/*	MethodMap::Iterator iter = methodMap.seek(Key(StrKey(ConstStrA(prototype))));
	if (iter.hasItems()) {
		const MethodMap::KeyValue &kv = iter.getNext();
		if (kv.key.first == prototype && kv.key.second > version) return kv.value;
	}
	return null;
*/

}

bool Dispatcher::CmpMethodPrototype::operator ()(const Key &sa, const Key &sb) const {
	ConstStrA a(sa);
	ConstStrA b(sb);

	natural l1 = a.length();
	natural l2 = b.length();
	natural p1 = 0;
	natural p2 = 0;
	while (p1 < l1 && p2 < l2) {
		if (a[p1] == b[p2]) {
			p1++;p2++;
		} else {
			switch (a[p1]) {
			case 'd': if (b[p2] != 'n' || b[p2] != 's') return false;break;
			case '?': break;
			case '*': if (p1) {
						 p1--;
				         if (b[p2] != a[p1])
				        	 return a[p1]<b[p2];
					  } else {
						  return a[p1]<b[p2];
					  }
						break;
			default: {
				switch (b[p2]) {
					case 'd': if (a[p1] != 'n' || b[p2] != 's') return true;break;
					case '?': break;
					case '*': if (p2) {
									p2--;
									if (b[p2] != a[p1])
										return a[p1]<b[p2];
								} else {
									  return a[p1]<b[p2];
							   }
								break;
					default:
						  return a[p1]<b[p2];

					}
				break;
			}
			}
			p1++;
			p2++;
		}
	}

	if (p1 < l1 && a[p1] != '*') return true;
	if (p2 < l2 && b[p2] != '*') return false;
	return false;
}

class Dispatcher::ResultObserver: public Future<Response>::IObserver, public Request {
public:
	ResultObserver(Promise<JSON::ConstValue> result, ILog *logService):outres(result),logService(logService) {}
	virtual void resolve(const Response &result) throw() {
		try {
			JSON::Builder::CObject r = json("id",this->id)
					("result",result.result)
					("error",JSON::getConstant(JSON::constNull));
			if (result.context != null) {
				r("context",result.context);
			}
			if (result.logOutput != null ) {
				logService->logMethod(*this->httpRequest,this->methodName,this->params,this->context,result.logOutput);
			}
			outres.resolve(r);
		} catch (...) {
			outres.rejectInCatch();
		}
		delete this;
	}
	virtual void resolve(const PException &e) throw() {
		try {
			JSON::ConstValue exceptionObj;
			RpcException *rpce = dynamic_cast<RpcException *>(e.get());
			if (rpce) {
				exceptionObj = rpce->getJSON(json);
			} else {
				exceptionObj = UncauchException(THISLOCATION,*e).getJSON(json);
			}

			JSON::Builder::CObject r = json("id",this->id)
					("error",exceptionObj)
					("result",JSON::getConstant(JSON::constNull));
			logService->logMethod(*this->httpRequest,this->methodName,this->params,this->context,exceptionObj);

			outres.resolve(r);
		} catch (...) {
			outres.rejectInCatch();
		}
		delete this;
	}

protected:
	Promise<JSON::ConstValue> outres;
	ILog *logService;

};

class Dispatcher::ExceptionTranslateObserver: public Future<Response>::IObserver {
public:
	ExceptionTranslateObserver(Request *r, Promise<Response> result)
		:r(r),outres(result) {}
	virtual void resolve(const Response &result) throw() {
		outres.resolve(result);
		delete this;
	}
	virtual void resolve(const PException &e) throw() {
		r->dispatcher->dispatchException(*r, e, outres);
		delete this;
	}

protected:
	Request *r;
	Promise<Response> outres;
	IDispatcher *dispatch;

};

void Dispatcher::dispatchMessage(const JSON::ConstValue jsonrpcmsg,
		const JSON::Builder &json, BredyHttpSrv::IHttpRequestInfo *request,
		Promise<JSON::ConstValue> result) throw()
 {

	//we will store response here - we will also use this object to store exception during dispatching
	Future<Response> respf;
	Promise<Response> respp = respf.getPromise();

	JSON::ConstValue id = jsonrpcmsg["id"];
	JSON::ConstValue method = jsonrpcmsg["method"];
	JSON::ConstValue params = jsonrpcmsg["params"];
	JSON::ConstValue context = jsonrpcmsg["context"];

	if (method == null) {
		respp.reject(ParseException(THISLOCATION,"Missing 'method'"));
	} else if (!method->isString()) {
		respp.reject(ParseException(THISLOCATION,"Method must be string"));
	} else if (params == null) {
		respp.reject(ParseException(THISLOCATION, "Missing 'params'"));
	} else {
		if (!params->isArray()) {
			params = json << params;
		}

		AllocPointer<ResultObserver> r = new ResultObserver(result, getLogObject());
		r->context = context;
		r->dispatcher = Pointer<IDispatcher>(this);
		r->httpRequest = request;
		r->id = id;
		r->isNotification = id == null || id->isNull();
		r->json = json;
		r->methodName = method.getStringA();
		r->params = params;

		callMethod(*r,respp);
		Future<Response> respf2;
		respf.addObserver(new ExceptionTranslateObserver(r,respf2.getPromise()));
		respf2.addObserver(r.detach());
	}
}

void Dispatcher::regMethodHandler(ConstStrA method, IMethod* fn) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	fn->enableMTAccess();
	methodMap.insert(StrKey(StringA(method)),fn);
}

void Dispatcher::unregMethod(ConstStrA method) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	methodMap.erase(StrKey(method));

}

void Dispatcher::setLogObject(ILog* log) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	logObject = log;
}


ILog* Dispatcher::getLogObject() const {
	return logObject;
}


class Dispatcher::ResolveException {
public:
	const Request& req;
	const PException& exception;
	Promise<Response> &result;

	ResolveException(const Request& req,const PException& exception,Promise<Response> &result)
		:req(req),exception(exception),result(result) {}
	bool operator()(const ExceptionMap::KeyValue &kv) const {
		return  kv.value->operator()(req, exception,result);
	}
};

void Dispatcher::dispatchException(const Request& req, const PException& exception, Promise<Response> result) throw () {
	Synchronized<RWLock::ReadLock> _(mapLock);
	exceptionMap.forEach(ResolveException(req,exception,result), Direction::forward);
}


void Dispatcher::regExceptionHandler(ConstStrA name, IExceptionHandler* fn) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	fn->enableMTAccess();
	exceptionMap.insert(StrKey(StringA(name)),fn);
}

void Dispatcher::unregExceptionHandler(ConstStrA name) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	exceptionMap.erase(StrKey(name));
}

void Dispatcher::enumMethods(const IMethodEnum& enm) const {
	Synchronized<RWLock::ReadLock> _(mapLock);
	for (MethodMap::Iterator iter = methodMap.getFwIter(); iter.hasItems();) {
		const Key &key = iter.getNext().key;
		enm(key);
	}
}


}

