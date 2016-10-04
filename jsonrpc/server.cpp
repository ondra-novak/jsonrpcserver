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

{
	setLogObject(this);
	loadConfig(cfg);
}

Server::Server(const Config& config)
	:HttpHandler(static_cast<IDispatcher &>(*this))

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

	loadConfig(cfg);

}

class OldFunctionStub: public IMethod {
public:
	OldFunctionStub(const jsonsrv::IRpcCall &method):mptr(method.clone()) {}
	virtual Future<Response> operator()(const Request &req) const throw() {
		Future<Response> out;
		try {
			WeakRefPtr<IPeer> peer = req.peer;
			if (peer == null) throw CanceledException(THISLOCATION);
			WeakRefPtr<IDispatcher> dispptr(req.dispatcher);
			jsonsrv::RpcRequest oldreq;
			oldreq.args = static_cast<const JSON::Value &>(req.params);
			oldreq.context = static_cast<const JSON::Value &>(req.context);
			oldreq.functionName = req.methodName.getStringA();
			oldreq.httpRequest = peer->getHttpRequest();
			oldreq.id = req.id.getStringA();
			oldreq.idnode = static_cast<const JSON::Value &>(req.id);
			oldreq.jsonFactory = req.json.factory;
			oldreq.serverStub = dispptr->getIfcPtr<jsonsrv::IJsonRpc>();
			JSON::Value oldres = (mptr)(&oldreq);
			out.getPromise().resolve(Response(oldres, oldreq.contextOut, oldreq.logOutput));
		} catch (...) {
			out.getPromise().rejectInCatch();
		}
		return out;
	}

protected:
	jsonsrv::RpcCall mptr;


};


void Server::OldAPI::registerMethod(ConstStrA methodName,const jsonsrv::IRpcCall& method, ConstStrA ) {

	owner.regMethodHandler(version, methodName,new OldFunctionStub(method));
}

void Server::OldAPI::eraseMethod(ConstStrA methodName) {
	owner.unregMethod(methodName);
}

void Server::OldAPI::registerGlobalHandler(ConstStrA ,const jsonsrv::IRpcCall& ) {}
void Server::OldAPI::eraseGlobalHandler(ConstStrA ) {}
void Server::OldAPI::registerMethodObsolete(ConstStrA ) {}

void Server::OldAPI::registerStatHandler(ConstStrA name,const jsonsrv::IRpcCall& method) {
	owner.stats.regMethodHandler(version, name, new OldFunctionStub(method));
}

void Server::OldAPI::eraseStatHandler(ConstStrA name ) {
	owner.stats.unregMethod(name);
}

void Server::OldAPI::setRequestMaxSize(natural ) {}

Server::OldAPI::CallResult Server::OldAPI::callMethod(BredyHttpSrv::IHttpRequestInfo* httpRequest,
				ConstStrA methodName, const JSON::Value& args,
					const JSON::Value& context, const JSON::Value& id) {


	class FakePeer: public IPeer {
	public:
		virtual BredyHttpSrv::IHttpRequestInfo *getHttpRequest() const {return req;}
		virtual ConstStrA getName() const {
			return req->getIfc<BredyHttpSrv::IHttpPeerInfo>().getPeerRealAddr();
		}
		virtual natural getPortIndex() const {
			return req->getIfc<BredyHttpSrv::IHttpPeerInfo>().getSourceId();
		}
		virtual IRpcNotify *getNotifySvc() const {
			return 0;
		}
		virtual void setContext(Context *ctx) {
			this->ctx = ctx;
		}
		virtual Context *getContext() const {
			return ctx;
		}
		virtual IClient *getClient() const {
			return 0;
		}

		ContextVar ctx;
		BredyHttpSrv::IHttpRequestInfo *req;

		FakePeer(BredyHttpSrv::IHttpRequestInfo *req):req(req) {}
	};


	FakePeer fakePeer(httpRequest);
	WeakRefTarget<IPeer> peer(&fakePeer);
	JSON::Builder json;
	Request req;
	req.context = context;
	req.params = args;
	req.id = id;
	req.methodName = json(methodName);
	req.isNotification = false;
	req.dispatcher = owner.getDispatcherWeakRef();
	req.peer = peer;
	req.json = json.factory;

	CallResult cres;
	Future<Response> resp = owner.callMethod(req);
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

