/*
 * jsonRpc.h
 *
 *  Created on: Sep 22, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_H_
#define JSONRPCSERVER_JSONRPC_H_

#include "../httpserver/httprequest.h"
#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/utils/json.h"
#include "lightspeed/base/sync/threadVar.h"
#include "rpchandler.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/memory/sharedPtr.h"
#include "ijsonrpc.h"
#include "lightspeed/base/containers/set.h"

namespace jsonsrv {

using namespace BredyHttpSrv;
using namespace LightSpeed;

class JsonRpc : public IHttpHandler, public IJsonRpc
{
	struct CmpMethodPrototype {
		bool operator()(ConstStrA a, ConstStrA b) const;
	};

	typedef StringPool<char> StrPool;
	typedef StrPool::Str Str;
	typedef Map<Str, SharedPtr<IRpcCall>, CmpMethodPrototype > HandlerMap;
	typedef Map<Str,Str> MethodHelp;
	typedef Set<Str, CmpMethodPrototype> ObsoleteMethods;

public:
	JsonRpc();
	virtual natural onRequest(IHttpRequest& request, ConstStrA vpath);
	virtual natural onData(IHttpRequest& request);
    virtual void registerGlobalHandler(ConstStrA methodUID, const IRpcCall &method);
    virtual void eraseGlobalHandler(ConstStrA methodUID);
    virtual void registerMethodObsolete(ConstStrA methodName);
    ///Register server's build methods
    /**
     * @param developMode true to include listmethods, help and ping. Otherwise they are excluded which causes the web-client useless
     */
    void registerServerMethods(bool developMode);
    void registerMethod(ConstStrA methodName, const IRpcCall & method, ConstStrA help = ConstStrA());
    void eraseMethod(ConstStrA methodName);
    void registerStatHandler(ConstStrA handlerName, const IRpcCall & method);
    void eraseStatHandler(ConstStrA handlerName);
    ///Enables Cross Origin HTTP Request
    /**
     * @param enable true to enable, false to disable.
     *
     * @note You can specify CORS allow-origin. Default is '*' so every client can access the server
     */
    void enableCORS(bool enable);
    ///Determines CORS allowed state
    /**
     * @retval true CORS is enabled
     * @retval false CORS is disabled
     */
    bool isCORSEnabled() const;
    ///Changes CORS origin
    /**
     * @param origin new CORS allow-origin. Function doesn't allow the CORS, you have to call enableCORS()
     */
    void setCORSOrigin(ConstStrA origin);
    ///Retrieves current CORS origin<
    ConstStrA getCORSOrigin() const;



    String getClientPage() const    {return clientPage;}
    void setClientPage(String clientPage){this->clientPage = clientPage;}

    void loadHelp(SeqFileInput &input);
    void loadHelp(ConstStrW filename);

    CallResult callMethod(IHttpRequest *httpRequest, ConstStrA methodName, JSON::INode *args, JSON::INode *context, JSON::INode *id);
    virtual void setLogObject(IJsonRpcLogObject *logObject) {this->logObject = logObject;}
    virtual void setRequestMaxSize(natural bytes);
    virtual RpcError onException(JSON::IFactory *json, const std::exception &e);



protected:
    natural loadHTMLClient(IHttpRequest& request);
    natural initJSONRPC(IHttpRequest& request);
    String clientPage;
    ThreadVarInitDefault<JSON::PFactory> jsonFactory;
    StrPool strings;
    HandlerMap methodMap;
    MethodHelp methodHelp;
    ObsoleteMethods obsoleteMap;
    HandlerMap globalHandlers;
    HandlerMap statHandlerMap;
    Pointer<IJsonRpcLogObject> logObject;
    natural maxRequestSize;

    bool corsEnabled;
    StringA corsOrigin;

    JSON::PNode parseRequest(IHttpRequest& request, JSON::IFactory *factory);
    JSON::PFactory getFactory();
	void replyError(String msg, IHttpRequest& request, JSON::PNode idnode );


	JSON::PNode rpcListMethods(  RpcRequest *rq);
	JSON::PNode rpcHelp(  RpcRequest *rq);
	JSON::PNode rpcPing( RpcRequest *rq);
	JSON::PNode rpcPingNotify( RpcRequest *rq);
	JSON::PNode rpcMulticall1(  RpcRequest *rq);
	JSON::PNode rpcMulticallN( RpcRequest *rq);
	JSON::PNode rpcStats( RpcRequest *rq);
	JSON::PNode rpcCrash( RpcRequest *rq);
	JSON::PNode rpcCrashScheduler( RpcRequest *rq);

	natural dumpMethods(ConstStrA name, IHttpRequest &request);
	natural sendClientJs(IHttpRequest &request);
	natural sendWsClientJs(IHttpRequest &request);


	StringA methodListTag;
};




} /* namespace jsonsrv */
#endif /* JSONRPCSERVER_JSONRPC_H_ */

