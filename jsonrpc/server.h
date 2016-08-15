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

namespace jsonrpc {

class Server: public ServerMethods, public Dispatcher, public HttpHandler, public ILog {
public:
	Server(const IniConfig &cfg);
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

				Config() :developMode(false), enableMulticall(true), enableListMethods(true),enableStats(true) {}
			};

	Server(const Config &config);

	///Perform logRotate of the rpclog
	void logRotate() {openLog();}

	virtual void regMethodHandler(ConstStrA method, IMethod *fn, natural untilVer=naturalNull);
	virtual void unregMethod(ConstStrA method, natural ver=naturalNull);
    virtual void regStatsHandler(ConstStrA name, IMethod *fn, natural untilVer=naturalNull);
    virtual void unregStats(ConstStrA name, natural ver=naturalNull);

	virtual natural onRequest(BredyHttpSrv::IHttpRequest& request, ConstStrA vpath);


protected:
	void loadConfig(const Config &cfg);
	void loadConfig(const IniConfig::Section &cfg);

	void openLog();
};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONRPCSERVER_165465416_SERVER_H_ */
