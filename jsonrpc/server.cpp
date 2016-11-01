/*
 * server.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include <lightspeed/utils/json/jsonserializer.h>
#include "server.h"
#include "lightspeed/base/debug/dbglog.h"
#include "errors.h"
#include "lightspeed/base/framework/app.h"
#include "../httpserver/httprequest.h"
#include "lightspeed/base/memory/smallAlloc.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/interface.tcc"
#include "lightspeed/base/actions/promise.tcc"

#include "ipeer.h"
using BredyHttpSrv::IHttpPeerInfo;
using LightSpeed::DbgLog::needRotateLogs;
using LightSpeed::JSON::serialize;
using LightSpeed::SmallAlloc;
namespace jsonrpc {

Server::Server(const IniConfig::Section& cfg)
	:HttpHandler(static_cast<IDispatcher &>(*this))
	,wsHandler(static_cast<IDispatcher &>(*this))
	,wsenabled(true)

{
	setLogObject(this);
	loadConfig(cfg);
}

Server::Server(const Config& config)
	:HttpHandler(static_cast<IDispatcher &>(*this))
	,wsHandler(static_cast<IDispatcher &>(*this))
	,wsenabled(true)

{
	setLogObject(this);
	loadConfig(config);
}

Server::~Server() {
	setLogObject(0);
}

void Server::regMethodHandler(natural version, ConstStrA method, IMethod* fn) {
	if (version == naturalNull)
		LS_LOG.info("(jsonrpc) add method: %1") << method;
	else
		LS_LOG.info("(jsonrpc) add method: %1 (up to version: %2)") << method << version;
	return Dispatcher::regMethodHandler(version, method,fn);
}

void Server::unregMethod(ConstStrA method) {
	LS_LOG.info("(jsonrpc) remove method: %1") << method;
	Dispatcher::unregMethod(method);
}

void Server::regStatsHandler(ConstStrA name, IMethod* fn) {
/*	if (untilVer != naturalNull)
		LS_LOG.info("(jsonrpc) add stat.handler: %1 ( <= %2)") << name << untilVer;
	else
		LS_LOG.info("(jsonrpc) add stat.handler: %1") << name;
	Dispatcher::regStatsHandler(name,fn,untilVer);*/

}

void Server::unregStats(ConstStrA name) {
}

/*natural Server::onRequest(BredyHttpSrv::IHttpRequest& request,
		ConstStrA vpath) {
}*/

void Server::loadConfig(const Config& cfg) {
	LogObject lg(THISLOCATION);
	logFileName = cfg.logFile;
	openLog();
	if (!cfg.helpDir.empty()) {
		try {
			PFolderIterator iter = IFileIOServices::getIOServices().openFolder(cfg.helpDir);
			while (iter->getNext()) {
				try {
				//	loadHelp(iter->getFullPath());
				} catch (Exception &e) {
					lg.warning("Skipping help file: %1 error %2") << iter->entryName << e.getMessage();
				}
			}
		} catch (Exception &e) {
			lg.warning("Unable to open help folder: %1") << e.getMessage();
		}
	}
	setClientPage(cfg.clientPage);
	registerServerMethods(*this,(cfg.developMode ? flagDevelopMode : 0)
		| (cfg.enableMulticall ? flagEnableMulticall : 0)
		| (cfg.enableListMethods ? flagEnableListMethods : 0)
		| (cfg.enableStats ? flagEnableStatHandler : 0));


	wsenabled = cfg.enableWebSockets;
/*	registerStatHandler("server",RpcCall::create(this,&JsonRpcServer::rpcHttpStatHandler));
	if (cfg.corsOrigin != nil) {
		setCORSOrigin((StringA)cfg.corsOrigin);
		enableCORS(true);
	}
	nullV = JSON::create()(null);*/

}

WeakRef<IDispatcher> Server::getDispatcherWeakRef() {
	return me;
}

void Server::loadConfig(const IniConfig::Section& sect) {
	StringA clientPage;
	StringA helpdir;
	StringA rpclogfile;


	FilePath p(App::current().getAppPathname());
	Config cfg;

	sect.get(clientPage, "clientPage");
	if (!clientPage.empty()) {
		cfg.clientPage = p / clientPage;
	}
	sect.get(helpdir,"helpDir");
	if (!helpdir.empty()) {
		cfg.helpDir = p / helpdir;
	}
	sect.get(rpclogfile,"rpcLog");
	if (!rpclogfile.empty()) {
		cfg.logFile = p / rpclogfile;
	}


	StringA corsOrigin;
	bool corsEnable = sect.get(corsOrigin, "allowCORSOrigins");
	if (corsEnable) cfg.corsOrigin = corsOrigin;
	sect.get(cfg.developMode, "developMode");
	sect.get(cfg.enableMulticall, "multicall");
	sect.get(cfg.enableListMethods, "listMethods");
	sect.get(cfg.enableStats, "enableStats");
	sect.get(cfg.enableWebSockets, "enableWebsockets");

	loadConfig(cfg);

}

natural Server::onGET(BredyHttpSrv::IHttpRequest& req, ConstStrA vpath) {
	if (vpath.empty() && req.getHeaderField(req.fldUpgrade) == "websocket" ) {
		if (!wsenabled) return stForbidden;
		else return req.forwardRequestTo(&wsHandler,vpath);
	} else {
		return HttpHandler::onGET(req,vpath);
	}
}

} /* namespace jsonrpc */

