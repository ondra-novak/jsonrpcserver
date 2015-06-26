/*
 * serverMain.h
 *
 *  Created on: Apr 15, 2015
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_BREDY_SERVERMAIN_H_
#define BREDYHTTPSERVER_BREDY_SERVERMAIN_H_
#include <lightspeed/utils/configParser.h>
#include "lightspeed/base/framework/serviceapp.h"

#include "../httpserver/httpServer.h"
#include "lightspeed/utils/FilePath.h"

namespace LightSpeed {
class ILogOutput;

}

namespace BredyHttpSrv {

using namespace LightSpeed;

class IHttpMapper;


///Class implements ServiceApp and represents empty JSONRPC server
/**
 *  This class is abstract and you have to inherit it and implement several methods.
 *  To implement server application, declare static instance of inherited class and make it as startup instance.
*/
class AbstractServerMain: public ServiceApp  {
public:
	///Called during server initialization
	/** at this point, server is not yet detached from the console.
	 *
	 * You should perform all initialization steps of your server, for example read and validate
	 * configuration and initialize services. You should avoid to modify any persistent state
	 * of the server. You should also avoid to perform long running calculations. On some
	 * platoforms, initialization can be executed twice - where first run is just validation
	 * step, because second run is executed on a different process which is started as a service.
	 *
	 * @param args arguments on command line without first three arguments.
	 * @param serr stdout where you can put error message or warning messages produced during initialization
	 * @param config parsed master config file
	 * @return upon succesful initialization server should return zero. Other value is treat as failure
	 */
	virtual natural onInitServer(const Args& args, SeqFileOutput serr, const IniConfig &config) = 0;

	///Caled during server start
	/** At this point, server is detached from console, server object is initialized, but port is not
	 * yet opened.
	 *
	 * You should now start all services and register them on various paths using httpMapper
	 *
	 * @param httpMapper interface to map your services in HTTP interface.
	 * @return upon succesful initialization server should return zero. Other value is treat as failure
	 *
	 * @note IHttpMapper implements IInterface. There are some services available through it,
	 * for example IJobScheduler, IJobManager and IHttpLiveLog
	 */
	virtual natural onStartServer(IHttpMapper &httpMapper) = 0;
	///Called after the server is initialized and serving content
	/**
	 * @param httpMapper interface to map additional services. You can anytime dynamically add
	 * or remove services from the server
	 */
	virtual void onServerReady(IHttpMapper &) {}

	///Caled when Stop command issued - but in this point, server is still running
	virtual void onStopServer(IHttpMapper &) {};

	///Called when server finished
	/** At this point, server is stopped, but interface is still valid
	 * It is not necesery remove services from the interface. They will be removed automatically
	 *
	 * @param httpMapper
	 */
	virtual void onServerFinish(IHttpMapper &) {};

	///Called on log rotate
	/** called after log rotating is performed
	 *
	 * Log rotating means, that all opened logs are renamed, so the server wants to reopen them, which causes creation of the new files.
	 * If server have multiple logs, you can reopen them in this function
	 *
	 * You don't need to implement log rotation, it is often handled by an external application (such a logrotate). You just need
	 *  to close and reopen the logs.
	 */
	virtual void onLogRotate() {}
protected:
    virtual integer onMessage(ConstStrA command, const Args & args, SeqFileOutput output);
    virtual void onThreadException(const Exception &e) throw();
	virtual void onException(const Exception &e);

    virtual integer initService(const Args & args, SeqFileOutput serr);
    virtual integer startService();


    ///Retrieves singleton to log object
    static ILogOutput &logOutputSingleton();

    void setUserGroup(ConstStrA usergroup);

protected:
    typedef AutoArray<natural> PortList;
	natural port;
	PortList otherPorts;
	HttpServer::Config serverConfig;
	bool started;
	StringA baseUrl;
	StringA serverIdent;
	lnatural exceptSumTm;
	StringA livelog;
	StringA livelog_realm;
	StringA livelog_userlist;
	String cfgPath;
	StringA usergroup;
	FilePath appPath;


	void readMainConfig(const IniConfig &cfg);
};

}




#endif /* BREDYHTTPSERVER_BREDY_SERVERMAIN_H_ */
