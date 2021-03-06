/*
 * httpServer.cpp
 *
 *  Created on: Sep 23, 2012
 *      Author: ondra
 */

#include "httpServer.h"
#include <lightspeed/base/containers/autoArray.tcc>
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/base/debug/livelog.h"
#include "queryParser.h"
#include "lightspeed/base/interface.tcc"
#include "lightspeed/base/streams/netio_ifc.h"
#include "lightspeed/utils/base64.h"
#include "lightspeed/base/containers/convertString.tcc"
#include "lightspeed/base/exceptions/netExceptions.h"
#include "livelog.h"


namespace BredyHttpSrv {


HttpServer::HttpServer(StringA serverIdent, const Config &config)
	:BredyHttpSrv::ConnHandler(serverIdent, config.maxBusyThreads)
	 ,threadPool(config)
	 ,tcplisten(*this,&threadPool)
	 , trustedProxies(config.trustedProxies)
{
	lastThreadCount = 0;
}

void HttpServer::start(natural port, bool bindLocal , natural connTimeout ) {

	tcplisten.start(port,bindLocal,connTimeout);
}

natural HttpServer::addPort(natural port, bool bindLocal , natural connTimeout ) {
	return tcplisten.addPort(port,bindLocal,connTimeout);
}

void HttpServer::stop() {

 	 tcplisten.stop();
}

ITCPServerContext* HttpServer::onConnect(const NetworkAddress& addr) throw() {
	ITCPServerContext* x = ConnHandler::onIncome(addr);
	reportThreadUsage();
	return x;
}

ConnHandler::Command HttpServer::onDataReady(const PNetworkStream& stream,
		ITCPServerContext* context) throw() {
	ParallelExecutor *executor = &threadPool;
	natural t = executor->getThreadCount();
	if (t != lastThreadCount) {
		DbgLog::setThreadName("server",false);
		reportThreadUsage();
		lastThreadCount = t;
	}
	return  ConnHandler::onDataReady(stream,context);
}

void HttpServer::executeInPool(const IThreadFunction &threadFn) {
	threadPool.execute(threadFn);
}

const void *HttpServer::proxyInterface(const IInterfaceRequest &p) const {
	if (typeid(IJobScheduler) == p.getType()) return static_cast<const IJobScheduler *>(&jobScheduler);
	return  BredyHttpSrv::ConnHandler::proxyInterface(p);
}
void *HttpServer::proxyInterface(IInterfaceRequest &p)  {
	if (typeid(IJobScheduler) == p.getType()) return static_cast<IJobScheduler *>(&jobScheduler);
	return  BredyHttpSrv::ConnHandler::proxyInterface(p);
}

void HttpServer::runJob(const LightSpeed::IThreadFunction& action) {
	executeInPool(action);
}

void HttpServer::reportThreadUsage() {
	ParallelExecutor *executor = &threadPool;
	LogObject(THISLOCATION).info("Workers: %1, Idle: %2, Connections: %3")
			<< executor->getThreadCount()
			<< executor->getIdleCount()
			<< tcplisten.getConnectionCount();
}


HttpServer::LocalParallelExecutor::LocalParallelExecutor(const Config& config)
:ParallelExecutor(config.maxThreads,naturalNull,config.newThreadTimeout,config.threadIdleTimeout)
{
}

void HttpServer::LocalParallelExecutor::onThreadInit() {
	LogObject(THISLOCATION).info("New worker thread");
}

void HttpServer::LocalParallelExecutor::onThreadDone() {
	LogObject(THISLOCATION).info("Worker thread exit");
}


void HttpServer::addLiveLog(ConstStrA path) {
	static LiveLogHandler handler;
	addSite(path,&handler);

}


void HttpServer::addLiveLog(ConstStrA path, ConstStrA realm, ConstStrA userList) {
	static LiveLogAuthHandler handler;
	handler.setRealm(realm);
	handler.setUserlist(userList);
	addSite(path,&handler);
}


void HttpServer::recordRequestDuration(natural duration) {
	double d = duration*0.001;
	ParallelExecutor *executor = &threadPool;
	stats.requests.add(1);
	stats.latency.add(d);
	stats.worktime.add(d);
	stats.connections.add(tcplisten.getConnectionCount());
	stats.threads.add(executor->getThreadCount());
	stats.idleThreads.add(executor->getIdleCount());
}

bool HttpServer::isPeerTrustedProxy(ConstStrA ip)
{
	return trustedProxies.find(ip) != naturalNull;
}


}

