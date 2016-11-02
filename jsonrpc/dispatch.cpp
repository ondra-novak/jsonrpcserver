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

#include "ilog.h"
#include "ipeer.h"
#include "methodreg.tcc"
namespace jsonrpc {

using namespace LightSpeed;

Dispatcher::Dispatcher():logObject(0),me(this) {}
Dispatcher::~Dispatcher() {
	logObject.setNull();
	me.setNull();
}

template<typename Container>
inline void Dispatcher::createPrototype(ConstStrA methodName, JValue params, Container& container) {

	container.append(methodName);
	container.add(':');
	for (auto &&v:params) {
		char c;
		switch (v.type()) {
			case json::array: c = 'a';break;
			case json::boolean: c = 'b';break;
			case json::object: c= 'c';break;
			case json::number: c = 'n';break;
			case json::null: c= 'x';break;
			case json::string: c = 's';break;
			default: c = 'u';break;
		}
		container.add(c);
	}
}

Future<Response> Dispatcher::callMethod(const Request& req) throw() {

	try {
		AutoArray<char, SmallAlloc<256> > prototype;

		createPrototype(~req.methodName.getString(),req.params,prototype);
		PMethodHandler m1 = findMethod(prototype);
		if (m1 == null) {
			m1 = findMethod(~req.methodName.getString());
			if (m1 == null) {
				throw LookupException(THISLOCATION,prototype);
			}
		}


		return (*m1)(req);
	} catch (...) {
		Future<Response> r;
		r.getPromise().rejectInCatch();
		return r;
	}

}


Dispatcher::PMethodHandler Dispatcher::findMethod(ConstStrA prototype, natural version) {
	Synchronized<RWLock::ReadLock> _(mapLock);

	const MethodDef *h = methodMap.find(StrKey(prototype));
	if (h == 0) return null;
	else {
		if (h->version < version) return null;
		return h->handler;
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

class Dispatcher::ResultObserver: public Future<Response>::IObserver, public Request, public DynObject {
public:
	ResultObserver(Promise<JValue> result, WeakRef<ILog> logService):outres(result),logService(logService) {}
	virtual void resolve(const Response &result) throw() {

		try {
			JObject r;
			r("id",this->id)
				("result",result.result)
				("error",JValue(nullptr));
			if (result.context != null) {
				r("context",result.context);
			}
			{
				WeakRefPtr<IPeer> peer(this->peer);
				WeakRefPtr<ILog> log(logService);
				if (peer != null && log != null) {
					log->logMethod(peer->getName(),~this->methodName.getString(),this->params,this->context,result.logOutput);
				}
			}
			outres.resolve(r);
		} catch (...) {
			outres.rejectInCatch();
		}
		delete this;
	}
	virtual void resolve(const PException &e) throw() {
		try {
			JValue exceptionObj;
			RpcException *rpce = dynamic_cast<RpcException *>(e.get());
			if (rpce) {
				exceptionObj = rpce->getJSON();
			} else {
				exceptionObj = UncauchException(THISLOCATION,*e).getJSON();
			}

			JObject r;
			r("id",this->id)
					("error",exceptionObj)
					("result",JValue(nullptr));
			{
				WeakRefPtr<IPeer> peer(this->peer);
				WeakRefPtr<ILog> log(logService);
				if (peer != null && log != null) {
					log->logMethod(peer->getName(),~this->methodName.getString(),this->params,this->context,exceptionObj);
				}
			}

			outres.resolve(r);
		} catch (...) {
			outres.rejectInCatch();
		}
		delete this;
	}

protected:
	Promise<JValue> outres;
	WeakRef<ILog> logService;

};

class Dispatcher::ExceptionTranslateObserver: public Future<Response>::IObserver, public DynObject {
public:
	ExceptionTranslateObserver(const Request &r, Promise<Response> result)
		:r(r),outres(result) {}
	virtual void resolve(const Response &result) throw() {
		outres.resolve(result);
		delete this;
	}
	virtual void resolve(const PException &e) throw() {
		WeakRefPtr<IDispatcher> dispptr(r.dispatcher);
		if (dispptr != null) {
			outres.resolve(dispptr->dispatchException(r, e));
		} else {
			outres.resolve(e);
		}
		delete this;
	}

protected:
	const Request &r;
	Promise<Response> outres;
	IDispatcher *dispatch;

};

Future<JValue> Dispatcher::dispatchMessage(const JValue jsonrpcmsg,const WeakRef<IPeer> &peer) throw()
 {


	JValue id = jsonrpcmsg["id"];
	JValue method = jsonrpcmsg["method"];
	JValue params = jsonrpcmsg["params"];
	JValue context = jsonrpcmsg["context"];

	//Request is initialized as ResultObserver which will later create result JSON
	Future<JValue> result;
	AllocPointer<ResultObserver> r = new(IPromiseControl::getAllocator()) ResultObserver(result.getPromise(), getLogObject());
	r->context = context;
	r->dispatcher = me;
	r->peer = peer;
	r->id = id;
	r->isNotification = !id.defined() || id.isNull();
	r->methodName = method;
	r->params = params;
	r->version = peer.lock()->getVersion();


	try {
		if (method == null) {
			throw ParseException(THISLOCATION,"Missing 'method'");
		} else if (method.type() != json::string) {
			throw ParseException(THISLOCATION,"Method must be string");
		} else if (!params.defined()) {
			throw ParseException(THISLOCATION, "Missing 'params'");
		} else {
			if (params.type() != json::array) {
				params = JValue({params});
			}


			//Call method, receive response
			Future<Response> resp = callMethod(*r);

			//function can return Future(null) to emit "false" as result
			if (!resp.hasPromise()) {
				resp.clear();
				resp.getPromise().resolve(JValue(false));
			}

			//If value is ready now, we can make shortcut
			if (resp.tryGetValueNoThrow()) {
				//directly convert value to JSON.
				resp.addObserver(r.detach());
			} else {
				//create intermediate future
				Future<Response> resp2 = dispatchException(*r, resp);
				//attach convertor to JSON
				resp2.addObserver(r.detach());
			}
			//return converted value
			return result;
		}
	} catch (...) {
		//create error response
		Future<Response> err;
		//reject response with exception
		err.getPromise().rejectInCatch();
		//convert exception to JSON
		err.addObserver(r.detach());
		//return the result
		return result;

	}
}

void Dispatcher::regMethodHandler(natural version, ConstStrA method, IMethod* fn) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	methodMap.insert(StrKey(StringA(method)),MethodDef(fn,version));

}

void Dispatcher::unregMethod(ConstStrA method) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	methodMap.erase(StrKey(method));

}

void Dispatcher::setLogObject(ILog* log) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	logObject = log;
}


WeakRef<ILog> Dispatcher::getLogObject() const {
	return logObject;
}


class Dispatcher::ResolveException {
public:
	const Request& req;
	const PException& exception;
	Future<Response> &r;

	ResolveException(const Request& req,const PException& exception,Future<Response> &r)
		:req(req),exception(exception),r(r) {}
	bool operator()(const ExceptionMap::KeyValue &kv) const {
		r = kv.value->operator ()(req, exception);
		if (r.hasPromise()) {
			return true;
		} else {
			return false;
		}
	}
};

Future<Response> Dispatcher::dispatchException(const Request& req, const PException& exception) throw () {
	Synchronized<RWLock::ReadLock> _(mapLock);
	Future<Response> r(null);
	exceptionMap.forEach(ResolveException(req,exception,r), Direction::forward);
	if (!r.hasPromise()) {
		r.clear();
		r.getPromise().reject(exception);
	}
	return r;
}

Future<Response> Dispatcher::dispatchException(const Request& req,
		Future<Response> result) throw () {

	//create intermediate future
	Future<Response> resp2;
	//attach exception conversion observer
	result.addObserver(new(IPromiseControl::getAllocator()) ExceptionTranslateObserver(req,resp2.getPromise()));

	return resp2;

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

void Dispatcher::enumMethods(const IMethodEnum& enm, natural version) const {
	Synchronized<RWLock::ReadLock> _(mapLock);
	for (MethodMap::Iterator iter = methodMap.getFwIter(); iter.hasItems();) {
		const MethodMap::KeyValue &kv = iter.getNext();
		if (kv.value.version >= version)
			enm(kv.key);
	}
}


}

