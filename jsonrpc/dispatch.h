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

#include "lightspeed/base/memory/sharedPtr.h"

#include "lightspeed/mt/rwlock.h"
namespace jsonrpc {

using namespace LightSpeed;


class Dispatcher: public IDispatcher, public IMethodRegister, public IInterface {
public:

	Dispatcher();

	virtual void regMethodHandler(ConstStrA method, IMethod *fn, natural untilVer=naturalNull);
	virtual void unregMethod(ConstStrA method, natural ver=naturalNull);
    virtual void setLogObject(ILog *logObject);
    virtual void callMethod(const Request &req, Promise<Response> result) throw();
    virtual void dispatchException(const Request &req, const PException &exception, Promise<Response> result) throw();
    virtual void dispatchMessage(const JSON::ConstValue jsonrpcmsg, natural version,
    		const JSON::Builder &json, BredyHttpSrv::IHttpRequestInfo *request,
			Promise<JSON::ConstValue> result) throw();
	virtual void regExceptionHandler(ConstStrA name, IExceptionHandler *fn, natural untilVer);
	virtual void unregExceptionHandler(ConstStrA name, natural ver);



    ILog *getLogObject() const;


    virtual void enumMethods(const IMethodEnum &enm) const;



protected:



	typedef StringKey<StringA> StrKey;
	typedef std::pair<StrKey, natural> Key;

	struct CmpMethodPrototype {
		bool operator()(const Key &a, const Key &b) const;
	};
	typedef RefCntPtr<IMethod> PMethodHandler;
	typedef RefCntPtr<IExceptionHandler> PExceptionHandler;

	typedef Map<Key, PMethodHandler, CmpMethodPrototype> MethodMap;
	typedef Map<Key, PExceptionHandler, CmpMethodPrototype> ExceptionMap;

	MethodMap methodMap;
	ExceptionMap exceptionMap;

	Pointer<ILog> logObject;
	mutable RWLock mapLock;


    template<typename Container>
    static void createPrototype(ConstStrA methodName, JSON::ConstValue params, Container &container);
    PMethodHandler findMethod(ConstStrA prototype, natural version);

    class ResultObserver;
    class ExceptionTranslateObserver;

    class ResolveException;
};


}


#endif /* JSONRPC_JSONRPCSERVER_DISPATCH_H_qpokdlpkqwp0xxmiwer */
