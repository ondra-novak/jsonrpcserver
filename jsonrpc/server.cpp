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

using BredyHttpSrv::IHttpPeerInfo;
using LightSpeed::DbgLog::needRotateLogs;
using LightSpeed::JSON::serialize;
using LightSpeed::SmallAlloc;
namespace jsonrpc {

Server::Server(const IniConfig::Section& cfg)
	:HttpHandler(static_cast<IDispatcher &>(*this))
	,oldAPI(*this)

{
	setLogObject(this);
	loadConfig(cfg);
}

Server::Server(const Config& config)
	:HttpHandler(static_cast<IDispatcher &>(*this))
	,oldAPI(*this)

{
	setLogObject(this);
	loadConfig(config);
}

void Server::regMethodHandler(ConstStrA method, IMethod* fn) {
	LS_LOG.info("(jsonrpc) add method: %1") << method;
	Dispatcher::regMethodHandler(method,fn);
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

/*	registerStatHandler("server",RpcCall::create(this,&JsonRpcServer::rpcHttpStatHandler));
	if (cfg.corsOrigin != nil) {
		setCORSOrigin((StringA)cfg.corsOrigin);
		enableCORS(true);
	}
	nullV = JSON::create()(null);*/

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

	loadConfig(cfg);

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


void Server::OldAPI::registerMethod(ConstStrA methodName,const jsonsrv::IRpcCall& method, ConstStrA ) {

	owner.regMethodHandler(methodName,new OldFunctionStub(method));
}

void Server::OldAPI::eraseMethod(ConstStrA methodName) {
	owner.unregMethod(methodName);
}

void Server::OldAPI::registerGlobalHandler(ConstStrA ,const jsonsrv::IRpcCall& ) {}
void Server::OldAPI::eraseGlobalHandler(ConstStrA ) {}
void Server::OldAPI::registerMethodObsolete(ConstStrA ) {}

void Server::OldAPI::registerStatHandler(ConstStrA handlerName,const jsonsrv::IRpcCall& method) {
}

void Server::OldAPI::eraseStatHandler(ConstStrA handlerName) {

}

void Server::OldAPI::setRequestMaxSize(natural ) {}

Server::OldAPI::CallResult Server::OldAPI::callMethod(BredyHttpSrv::IHttpRequestInfo* httpRequest,
				ConstStrA methodName, const JSON::Value& args,
					const JSON::Value& context, const JSON::Value& id) {


	JSON::Builder json;
	Request req;
	req.context = context;
	req.params = args;
	req.id = id;
	req.methodName = methodName;
	req.isNotification = false;
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

Optional<bool> Server::OldAPI::isAllowedOrigin(ConstStrA /*origin*/) {
	return null;
}



} /* namespace jsonrpc */

