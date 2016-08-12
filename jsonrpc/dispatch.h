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

	static const natural flagDevelopMode = 1;
	static const natural flagEnableMulticall = 2;
	static const natural flagEnableListMethods = 4;
	static const natural flagEnableStatHandler = 8;

	Dispatcher();

    ///Register server's build methods
    /**
     * @param developMode true to include listmethods, help and ping. Otherwise they are excluded which causes the web-client useless
     */
    void registerServerMethods(natural flags);
	virtual void regMethodHandler(ConstStrA method, IMethod *fn, natural untilVer=naturalNull);
	virtual void unregMethod(ConstStrA method, natural ver=naturalNull);
    virtual void regStatsHandler(ConstStrA name, IMethod *fn, natural untilVer=naturalNull);
    virtual void unregStats(ConstStrA name, natural ver=naturalNull);
    virtual void setLogObject(ILog *logObject);
    virtual void callMethod(const Request &req, Promise<Response> result) throw();
    virtual JSON::ConstValue dispatchMessage(const JSON::ConstValue jsonrpcmsg, natural version, const JSON::Builder &json, BredyHttpSrv::IHttpRequestInfo *request, Promise<Response> result) throw();

    ILog *getLogObject() const;


    virtual void enumMethods(const IMethodEnum &enm) const;

    class OldAPI: public jsonsrv::IJsonRpc {
    public:
		///old interface - emulate it
		virtual void registerMethod(ConstStrA methodName, const jsonsrv::IRpcCall & method, ConstStrA help = ConstStrA());
		///old interface - emulate it
		virtual void eraseMethod(ConstStrA methodName);
		///old interface - not implemented
		virtual void registerGlobalHandler(ConstStrA methodUID, const jsonsrv::IRpcCall & method);
		///old interface - not implemented
		virtual void eraseGlobalHandler(ConstStrA methodUID);
		///old interface - not implemented
		virtual void registerMethodObsolete(ConstStrA methodName);
		///old interface - emulate it
		virtual void registerStatHandler(ConstStrA handlerName, const jsonsrv::IRpcCall & method);
		///old interface - emulate it
		virtual void eraseStatHandler(ConstStrA handlerName);
		///old interface - not implemented
		virtual void setRequestMaxSize(natural bytes);
		///old interface - emulate it
		virtual CallResult callMethod(BredyHttpSrv::IHttpRequestInfo *httpRequest, ConstStrA methodName, const JSON::Value &args, const JSON::Value &context, const JSON::Value &id);
		///old interface - not implemented - return null
		virtual Optional<bool> isAllowedOrigin(ConstStrA origin) ;

	    virtual void setLogObject(ILog *logObject) {owner.setLogObject(logObject);}


		OldAPI(Dispatcher &owner):owner(owner) {}

	    Dispatcher &owner;

    };

    OldAPI oldAPI;


protected:







	typedef StringKey<StringA> StrKey;
	typedef std::pair<StrKey, natural> Key;

	struct CmpMethodPrototype {
		bool operator()(const Key &a, const Key &b) const;
	};
	typedef RefCntPtr<IMethod> PHandler;



	typedef MultiMap<Key, PHandler, CmpMethodPrototype> MethodMap;

	typedef Map<StrKey,StringA> MethodHelp;

	MethodMap methodMap,statHandlers;
	Pointer<ILog> logObject;
	mutable RWLock mapLock;


    template<typename Container>
    static void createPrototype(ConstStrA methodName, JSON::ConstValue params, Container &container);
    PHandler findMethod(ConstStrA prototype, natural version);
};


}


#endif /* JSONRPC_JSONRPCSERVER_DISPATCH_H_qpokdlpkqwp0xxmiwer */
