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
namespace jsonrpc {

using namespace LightSpeed;


class Dispatcher: public IDispatcher, public IMethodRegister, public IInterface {
public:

	static const natural flagDevelopMode = 1;
	static const natural flagEnableMulticall = 2;
	static const natural flagEnableListMethods = 4;
	static const natural flagEnableStatHandler = 8;

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



protected:

    const IMethod *findMethod(ConstStrA prototype, natural version);

    template<typename Container>
    static void createPrototype(ConstStrA methodName, JSON::ConstValue params, Container &container);





	typedef StringKey<StringA> StrKey;
	typedef std::pair<StrKey, natural> Key;

	struct CmpMethodPrototype {
		bool operator()(const Key &a, const Key &b) const;
	};
	typedef SharedPtr<IMethod> PHandler;

	typedef MultiMap<Key, PHandler, CmpMethodPrototype> MethodMap;

	typedef Map<StrKey,StringA> MethodHelp;

	MethodMap methodMap;


};


}


#endif /* JSONRPC_JSONRPCSERVER_DISPATCH_H_qpokdlpkqwp0xxmiwer */
