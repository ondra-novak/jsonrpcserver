/*
 * dispatch.h
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_DISPATCH_H_qpokdlpkqwp0xxmiwer
#define JSONRPC_JSONRPCSERVER_DISPATCH_H_qpokdlpkqwp0xxmiwer
#include <lightspeed/base/interface.h>

#include "idispatch.h"
#include "lightspeed/base/containers/stringKey.h"

#include "lightspeed/base/containers/map.h"


#include "lightspeed/mt/rwlock.h"
#include "lightspeed/base/memory/weakref.h"

#include "methodreg.h"
namespace jsonrpc {

using namespace LightSpeed;


class Dispatcher: public IDispatcher, public IMethodRegister {
public:

	Dispatcher();
	~Dispatcher();

	virtual void regMethodHandler(natural version, ConstStrA method, IMethod *fn);
	virtual void unregMethod(ConstStrA method);
    virtual void setLogObject(ILog *logObject);
    virtual Future<Response> callMethod(const Request &req) throw();
    virtual Future<Response> dispatchException(const Request &req, const PException &exception) throw();
    virtual Future<JValue> dispatchMessage(const JValue jsonrpcmsg,const WeakRef<IPeer> &peer) throw();
    virtual Future<Response> dispatchException(const Request &req,
    				Future<Response> result) throw();
	virtual void regExceptionHandler(ConstStrA name, IExceptionHandler *fn);
	virtual void unregExceptionHandler(ConstStrA name);



    WeakRef<ILog> getLogObject() const;


    virtual void enumMethods(const IMethodEnum &enm, natural version) const;

	typedef RefCntPtr<IMethod> PMethodHandler;
    PMethodHandler findMethod(StrView prototype, natural version = 0);



protected:



	typedef StringKey<StringA> StrKey;
	typedef StrKey Key;

	struct CmpMethodPrototype {
		bool operator()(const Key &a, const Key &b) const;
	};
	typedef RefCntPtr<IExceptionHandler> PExceptionHandler;

	struct MethodDef {
	public:
		PMethodHandler handler;
		natural version;

		MethodDef(PMethodHandler handler, natural version):handler(handler),version(version) {}
	};

	typedef Map<Key, MethodDef, CmpMethodPrototype> MethodMap;
	typedef Map<Key, PExceptionHandler, CmpMethodPrototype> ExceptionMap;

	MethodMap methodMap;
	ExceptionMap exceptionMap;

	WeakRefTarget<ILog> logObject;
	WeakRefTarget<IDispatcher> me;
	mutable RWLock mapLock;


    template<typename Container>
    static void createPrototype(StrView methodName, JValue params, Container &container);

    class ResultObserver;
    class ExceptionTranslateObserver;

    class ResolveException;
};


}


#endif /* JSONRPC_JSONRPCSERVER_DISPATCH_H_qpokdlpkqwp0xxmiwer */
