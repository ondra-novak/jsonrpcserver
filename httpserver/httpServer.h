/*
 * httpServer.h
 *
 *  Created on: Sep 23, 2012
 *      Author: ondra
 */

#ifndef BREDYHTTP_HTTPSERVER_H_
#define BREDYHTTP_HTTPSERVER_H_

#include "ConnHandler.h"
#include "lightspeed/base/framework/TCPServer.h"
#include "IJobScheduler.h"
#include "JobSchedulerImpl.h"
#include "serverStats.h"

namespace BredyHttpSrv{

class HttpServer:  public BredyHttpSrv::ConnHandler,
					public IJobManager,
					public IHttpLiveLog,
					public IServerSvcs {
public:
	struct Config {
		natural maxThreads;
		natural maxBusyThreads;
		natural newThreadTimeout;
		natural threadIdleTimeout;
		//a comma separated list of proxies that are trusted
		StringA trustedProxies;
	};

	HttpServer( StringA serverIdent, const Config &config);

	void start(natural port, bool bindLocal = false, natural connTimeout = 30000);
	natural addPort(natural port, bool bindLocal = false, natural connTimeout = 30000);
	void stop();
	virtual ITCPServerContext *onConnect(const NetworkAddress &addr) throw();
	virtual ConnHandler::Command onDataReady(const PNetworkStream &stream, ITCPServerContext *context)  throw();

	virtual void executeInPool(const IThreadFunction &threadFn);
	virtual void runJob(const LightSpeed::IThreadFunction &action);


	const HttpStats &getStats() const {return stats;}

	virtual void addLiveLog(ConstStrA path);
	virtual void addLiveLog(ConstStrA path, ConstStrA realm, ConstStrA userList ) ;


	virtual const void *proxyInterface(const IInterfaceRequest &p) const;
	virtual void *proxyInterface(IInterfaceRequest &p);

	virtual bool isPeerTrustedProxy(ConstStrA ip);

	TCPServer &getServer() {return tcplisten;}
	const TCPServer &getServer() const {return tcplisten;}

	virtual IExecutor &getExecutor() {return *tcplisten.getExecutor();}
	virtual PNetworkEventListener getNetworkListener() {return tcplisten.getEventListener();}
	virtual IJobScheduler &getScheduler() {return jobScheduler;}



protected:

	class LocalParallelExecutor: public ParallelExecutor {
	public:
		LocalParallelExecutor(const Config &cfg);
		virtual void onThreadInit();
		virtual void onThreadDone();
	};

	LocalParallelExecutor threadPool;
	TCPServer tcplisten;
	natural lastThreadCount;
	JobSchedulerImpl jobScheduler;
	HttpStats stats;
	natural lastIndex;
	StringA trustedProxies;

	void reportThreadUsage();
	virtual void recordRequestDuration(natural duration);
	void initTrustedProxies(ConstStrA proxies);


};

} /* namespace jsonsrv */
#endif /* BREDYHTTP_HTTPSERVER_H_ */
