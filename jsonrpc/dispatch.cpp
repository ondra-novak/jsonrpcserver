/*
 * dispatch.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "dispatch.h"

#include "errors.h"

#include "lightspeed/base/containers/map.tcc"
namespace jsonrpc {

using namespace LightSpeed;

Dispatcher::Dispatcher():oldAPI(*this) {}

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
	PHandler m1 = findMethod(prototype, req.version);
	if (m1 == null) {
		m1 = findMethod(req.methodName, req.version);
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

Dispatcher::PHandler Dispatcher::findMethod(ConstStrA prototype, natural version) {
	Synchronized<RWLock::ReadLock> _(mapLock);


	MethodMap::Iterator iter = methodMap.seek(Key(StrKey(ConstStrA(prototype)),version+1));
	if (iter.hasItems()) {
		const MethodMap::KeyValue &kv = iter.getNext();
		if (kv.key.first == prototype && kv.key.second > version) return kv.value;
	}
	return null;


}

bool Dispatcher::CmpMethodPrototype::operator ()(const Key &sa, const Key &sb) const {
	ConstStrA a(sa.first);
	ConstStrA b(sb.first);

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
	return sa.second < sb.second;
}

JSON::ConstValue Dispatcher::dispatchMessage(const JSON::ConstValue jsonrpcmsg,
	natural version, const JSON::Builder& json,
	BredyHttpSrv::IHttpRequestInfo* request,
	Promise<Response> result) throw () {

	JSON::ConstValue id = jsonrpcmsg["id"];
	JSON::ConstValue method = jsonrpcmsg["method"];
	JSON::ConstValue params = jsonrpcmsg["params"];
	JSON::ConstValue context = jsonrpcmsg["context"];

	if (method == null) {
		result.reject(ParseException(THISLOCATION,"Missing 'method'"));
		return id;
	} else if (!method->isString()) {
		result.reject(ParseException(THISLOCATION,"Method must be string"));
		return id;
	} else if (params == null) {
		result.reject(ParseException(THISLOCATION, "Missing 'params'"));
		return id;
	} else if (!params->isArray()) {
		params = json << params;
	}

	Request r;
	r.context = context;
	r.dispatcher = Pointer<IDispatcher>(this);
	r.httpRequest = request;
	r.id = id;
	r.isNotification = id == null || id->isNull();
	r.json = json;
	r.methodName = method.getStringA();
	r.params = params;
	r.version = version;

	callMethod(r,result);
	return id;
}

void Dispatcher::registerServerMethods(natural) {}

void Dispatcher::regMethodHandler(ConstStrA method, IMethod* fn, natural untilVer) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	fn->enableMTAccess();
	methodMap.insert(Key(StrKey(StringA(method)),untilVer),fn);
}

void Dispatcher::unregMethod(ConstStrA method, natural ver) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	methodMap.erase(Key(StrKey(method),ver));

}

void Dispatcher::regStatsHandler(ConstStrA name, IMethod* fn,natural untilVer) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	fn->enableMTAccess();
	statHandlers.insert(Key(StrKey(StringA(name)),untilVer),fn);
}

void Dispatcher::unregStats(ConstStrA name, natural ver) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	methodMap.erase(Key(StrKey(name),ver));
}

void Dispatcher::setLogObject(ILog* log) {
	Synchronized<RWLock::WriteLock> _(mapLock);
	logObject = log;
}


ILog* Dispatcher::getLogObject() const {
	return logObject;
}

void Dispatcher::enumMethods(const IMethodEnum& enm) const {
	Synchronized<RWLock::ReadLock> _(mapLock);
	for (MethodMap::Iterator iter = methodMap.getFwIter(); iter.hasItems();) {
		const Key &key = iter.getNext().key;
		enm(key.first, key.second);
	}
}

class OldFunctionStub: public IMethod {
public:
	OldFunctionStub(const jsonsrv::IRpcCall &method):mptr(method.clone()) {}
	virtual void operator()(const Request &req, Promise<Response> res) const throw() {
		jsonsrv::RpcRequest oldreq;
		oldreq.args = static_cast<const JSON::Value &>(req.params);
		oldreq.context = static_cast<const JSON::Value &>(req.context);
		oldreq.functionName = req.methodName;
		oldreq.httpRequest = req.httpRequest;
		oldreq.id = req.id.getStringA();
		oldreq.idnode = static_cast<const JSON::Value &>(req.id);
		oldreq.jsonFactory = req.json.factory;
		oldreq.serverStub = req.dispatcher->getIfcPtr<jsonsrv::IJsonRpc>();
		JSON::Value oldres = (mptr)(&oldreq);
		res.resolve(Response(oldres,oldreq.contextOut, oldreq.logOutput));
	}

protected:
	jsonsrv::RpcCall mptr;


};


void Dispatcher::OldAPI::registerMethod(ConstStrA methodName,const jsonsrv::IRpcCall& method, ConstStrA ) {

	owner.regMethodHandler(methodName,new OldFunctionStub(method));
}

void Dispatcher::OldAPI::eraseMethod(ConstStrA methodName) {
	owner.unregMethod(methodName);
}

void Dispatcher::OldAPI::registerGlobalHandler(ConstStrA ,const jsonsrv::IRpcCall& ) {}
void Dispatcher::OldAPI::eraseGlobalHandler(ConstStrA ) {}
void Dispatcher::OldAPI::registerMethodObsolete(ConstStrA ) {}

void Dispatcher::OldAPI::registerStatHandler(ConstStrA handlerName,const jsonsrv::IRpcCall& method) {
	owner.regStatsHandler(handlerName,new OldFunctionStub(method));
}

void Dispatcher::OldAPI::eraseStatHandler(ConstStrA handlerName) {
	owner.unregStats(handlerName);

}

void Dispatcher::OldAPI::setRequestMaxSize(natural ) {}

Dispatcher::OldAPI::CallResult Dispatcher::OldAPI::callMethod(BredyHttpSrv::IHttpRequestInfo* httpRequest,
				ConstStrA methodName, const JSON::Value& args,
					const JSON::Value& context, const JSON::Value& id) {


	JSON::Builder json;
	Request req;
	req.context = context;
	req.params = args;
	req.id = id;
	req.methodName = methodName;
	req.isNotification = false;
	req.version = 1;
	req.dispatcher = &owner;
	req.httpRequest = httpRequest;
	req.json = json.factory;
	Future<Response> resp;
	CallResult cres;
	owner.callMethod(req,resp);
	cres.id = id;
	try {
		const Response &result = resp.getValue();
		cres.error = JSON::getConstant(JSON::constNull);
		cres.result = static_cast<const JSON::Value &>(result.result);
		cres.newContext = static_cast<const JSON::Value &>(result.context);
		cres.logOutput = result.logOutput;
		return cres;
	} catch (jsonsrv::RpcError &e) {
		cres.error = e.getError();
		cres.result = JSON::getConstant(JSON::constNull);
		cres.logOutput = cres.error;
		return cres;
	} catch (RpcException &e) {
		cres.error = const_cast<JSON::INode *>(e.getJSON(json).get());
		cres.result = JSON::getConstant(JSON::constNull);
		cres.logOutput = cres.error;
		return cres;
	}
}

Optional<bool> Dispatcher::OldAPI::isAllowedOrigin(ConstStrA /*origin*/) {
	return null;
}

}

