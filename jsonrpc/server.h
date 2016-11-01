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
