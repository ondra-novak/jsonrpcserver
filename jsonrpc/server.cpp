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
using BredyHttpSrv::IHttpPeerInfo;
using LightSpeed::DbgLog::needRotateLogs;
using LightSpeed::JSON::serialize;
using LightSpeed::SmallAlloc;
namespace jsonrpc {

Server::Server(const IniConfig::Section& cfg)
	:HttpHandler(static_cast<IDispatcher &>(*this))
	,oldAPI(*this)
	,logfile(null)
,logRotateCounter(0)

{
	setLogObject(this);
	loadConfig(cfg);
}

Server::Server(const Config& config)
	:HttpHandler(static_cast<IDispatcher &>(*this))
	,oldAPI(*this)
	,logfile(null)
,logRotateCounter(0)

{
	setLogObject(this);
	loadConfig(config);
}

void Server::regMethodHandler(ConstStrA method, IMethod* fn, natural untilVer) {
	if (untilVer != naturalNull)
		LS_LOG.info("(jsonrpc) add method: %1 ( <= %2)") << method << untilVer;
	else
		LS_LOG.info("(jsonrpc) add method: %1") << method;
	Server::regMethodHandler(method,fn,untilVer);
}

void Server::unregMethod(ConstStrA method, natural ver) {
	if (ver != naturalNull)
		LS_LOG.info("(jsonrpc) remove method: %1 ( <= %2)") << method << ver;
	else
		LS_LOG.info("(jsonrpc) remove method: %1") << method;
	Server::unregMethod(method,ver);
}

void Server::regStatsHandler(ConstStrA name, IMethod* fn,	natural untilVer) {
	if (untilVer != naturalNull)
		LS_LOG.info("(jsonrpc) add stat.handler: %1 ( <= %2)") << name << untilVer;
	else
		LS_LOG.info("(jsonrpc) add stat.handler: %1") << name;
	Server::regStatsHandler(name,fn,untilVer);

}

void Server::unregStats(ConstStrA name, natural ver) {
}

natural Server::onRequest(BredyHttpSrv::IHttpRequest& request,
		ConstStrA vpath) {
}

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

void Server::logMethod(BredyHttpSrv::IHttpRequestInfo& invoker,
		ConstStrA methodName, const JSON::ConstValue& params,
		const JSON::ConstValue& context, const JSON::ConstValue& logOutput) {
	IHttpPeerInfo &pinfo = invoker.getIfc<IHttpPeerInfo>();
	ConstStrA peerAddr = pinfo.getPeerRealAddr();
	logMethod(peerAddr,methodName,params,context,logOutput);

}

void Server::logMethod(ConstStrA source, ConstStrA methodName,
		const JSON::ConstValue& params, const JSON::ConstValue& context,
		const JSON::ConstValue& logOutput) {

	if (logfile == nil) return;
	if (logRotateCounter != DbgLog::rotateCounter) {
		if (DbgLog::needRotateLogs(logRotateCounter)) {
			logRotate();
		}
	}

	StringPool<char, SmallAlloc<4096> > buffer;
	typedef typename StringPool<char, SmallAlloc<4096> >::Str Str;
	typedef typename StringPool<char, SmallAlloc<4096> >::WriteIterator WrItr;

	WrItr writr = buffer.getWriteIterator();
	if (params == 0)
		JSON::serialize(JSON::getConstant(JSON::constNull),writr,false);
	else
		JSON::serialize(params,writr,false);

	Str strparams = writr.finish();

	if (context == 0)
		JSON::serialize(JSON::getConstant(JSON::constNull),writr,false);
	else
		JSON::serialize(context,writr,false);

	Str strcontext = writr.finish();

	if (logOutput == 0)
		JSON::serialize(JSON::getConstant(JSON::constNull),writr,false);
	else
		JSON::serialize(logOutput,writr,false);;

	Str stroutput = writr.finish();


	LogObject lg(THISLOCATION);
/*
	ConstStrA resparamstr(buffers.strparams.getArray());
	ConstStrA rescontextptr(buffers.strcontext.getArray());
	ConstStrA resoutputptr(buffers.stroutput.getArray());
	Synchronized<FastLock> _(lock);
	PrintTextA pr(*logfile);
	AbstractLogProvider::LogTimestamp tms;


	pr("%{04}1/%{02}2/%{02}3 %{02}4:%{02}5:%{02}6 - [\"%7\",\"%8\",%9,%10,%11]\n")
		<< tms.year << tms.month << tms.day
		<< tms.hour << tms.min << tms.sec
		<< source << methodName << resparamstr << rescontextptr << resoutputptr;
	logfile->flush();*/
}

void Server::openLog() {
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
	req.version = 1;
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

