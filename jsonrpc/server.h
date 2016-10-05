/*
 * server.h
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_165465416_SERVER_H_
#define JSONRPC_JSONRPCSERVER_165465416_SERVER_H_

#include <lightspeed/utils/configParser.h>
#include "../httpserver/httprequest.h"
#include "dispatch.h"
#include "httpHandler.h"
#include "serverMethods.h"

#include "lightspeed/base/streams/fileio.h"

#include "logService.h"
#include "wsHandler.h"
namespace jsonrpc {

class Server: public ServerMethods, public Dispatcher, public HttpHandler, public LogService {
public:
	Server(const IniConfig::Section &cfg);

	struct Config {
				FilePath logFile;
				FilePath helpDir;
				FilePath clientPage;
				Optional<StringA> corsOrigin;
				bool developMode;
				bool enableMulticall;
				bool enableListMethods;
				bool enableStats;
				bool enableWebSockets;

				Config() :developMode(false), enableMulticall(true), enableListMethods(true),enableStats(true),enableWebSockets(true) {}
			};

	Server(const Config &config);

	~Server();

	///Perform logRotate of the rpclog
	void logRotate() {openLog();}

	virtual void regMethodHandler(natural version, ConstStrA method, IMethod *fn);
	virtual void unregMethod(ConstStrA method);
    virtual void regStatsHandler(ConstStrA name, IMethod *fn);
    virtual void unregStats(ConstStrA name);

//	virtual natural onRequest(BredyHttpSrv::IHttpRequest& request, ConstStrA vpath);




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

	    virtual void setLogObject(jsonsrv::IJsonRpcLogObject *logObject) {owner.setLogObject(logObject);}


		OldAPI(Server &owner, natural version):owner(owner),version(version) {}

		Server &owner;
		const natural version;

    };

    OldAPI getOldApi(natural version = naturalNull) {
    	return OldAPI(*this,version);
    }

	WeakRef<IDispatcher> getDispatcherWeakRef();



protected:
	void loadConfig(const Config &cfg);
	void loadConfig(const IniConfig::Section &cfg);

	virtual natural onGET(BredyHttpSrv::IHttpRequest& req,  ConstStrA vpath);

	WSHandler wsHandler;
	bool wsenabled;

};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONRPCSERVER_165465416_SERVER_H_ */
