/*
 * server.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#include "server.h"

#include "lightspeed/base/debug/dbglog.h"
namespace jsonrpc {

Server::Server(const IniConfig& cfg):HttpHandler(static_cast<IDispatcher &>(*this)) {
	setLogObject(this);
	loadConfig(cfg.openSection("server"));

}

Server::Server(const IniConfig::Section& cfg):HttpHandler(static_cast<IDispatcher &>(*this)) {
	setLogObject(this);
	loadConfig(cfg);
}

Server::Server(const Config& config):HttpHandler(static_cast<IDispatcher &>(*this)) {
	setLogObject(this);
	loadConfig(config);
}

void Server::regMethodHandler(ConstStrA method, IMethod* fn, natural untilVer) {
	if (untilVer != naturalNull)
		LS_LOG.info("(jsonrpc) add method: %1 ( <= %2)") << method << untilVer;
	else
		LS_LOG.info("(jsonrpc) add method: %1") << method;
	Dispatcher::regMethodHandler(method,fn,untilVer);
}

void Server::unregMethod(ConstStrA method, natural ver) {
	if (ver != naturalNull)
		LS_LOG.info("(jsonrpc) remove method: %1 ( <= %2)") << method << ver;
	else
		LS_LOG.info("(jsonrpc) remove method: %1") << method;
	Dispatcher::unregMethod(method,ver);
}

void Server::regStatsHandler(ConstStrA name, IMethod* fn,	natural untilVer) {
	if (untilVer != naturalNull)
		LS_LOG.info("(jsonrpc) add stat.handler: %1 ( <= %2)") << name << untilVer;
	else
		LS_LOG.info("(jsonrpc) add stat.handler: %1") << name;
	Dispatcher::regStatsHandler(name,fn,untilVer);

}

void Server::unregStats(ConstStrA name, natural ver) {
}

natural Server::onRequest(BredyHttpSrv::IHttpRequest& request,
		ConstStrA vpath) {
}

void Server::loadConfig(const Config& cfg) {
}

void Server::loadConfig(const IniConfig::Section& cfg) {
}

void Server::openLog() {
}


} /* namespace jsonrpc */

