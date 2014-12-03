/*
 * ServerMain.h
 *
 *  Created on: Sep 1, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_ABSTRACTJSONRPCSERVER_H_
#define JSONRPCSERVER_ABSTRACTJSONRPCSERVER_H_
#include "lightspeed/base/framework/serviceapp.h"
#include "../httpserver/ConnHandler.h"
#include "lightspeed/base/framework/TCPServer.h"
#include "lightspeed/base/debug/dbglog.h"
#include <lightspeed/utils/configParser.h>
#include "../httpserver/httprequest.h"
#include "../httpserver/httpServer.h"
#include "lightspeed/utils/FilePath.h"



namespace BredyHttpSrv {
	class IHttpMapper;
}


namespace LightSpeed { namespace JSON {
class IFactory;
}}

namespace jsonsrv {
class RpcError;

using namespace LightSpeed;

class IJsonRpc;
class JsonHttpServer;

///Class implements ServiceApp and represents empty JSONRPC server
/**
 *  This class is abstract and you have to inherit it and implement several methods.
 *  To implement server application, declare static instance of inherited class and make it as startup instance.
*/
class AbstractJSONRPCServer: public LightSpeed::ServiceApp  {
public:


	///Called to initialize server
	/**
	 * application should read configuration, validate settings and report errors to the error output. It can
	 * prepare objects which will be later bound with the server. At this point, server instance doesn't exist yet
	 *
	 * @param args arguments passed from command line. First three arguments are stripped.
	 * @param serr object that contains ouput stream connected with error output. Put error messages here. They will
	 *  be displayed in console during startup
	 * @param config reference to configuration file.
	 * @return function should return 0 to continue initialization. If returned different value, server stops
	 * initialization and exits with exit code equal to returned value
	 */
	virtual natural onInitServer(const Args& args, SeqFileOutput serr, const IniConfig &config) = 0;

	///Called during server's start
	/**
	 * Called after server is created, but before socked it installed.
	 *
	 * @param jsonServer reference to JSON server. Interface have to be used to register JSONRPC methods. Note that
	 * reference to the interface can be temporary, so do not store pointer to this reference.
	 * @param httpMapper reference to HTTP mapped allowing to install custom HTTP handlers. Note that
	 * reference to the interface can be temporary, so do not store pointer to this reference.
	 * @return function should return 0
	 */
	virtual natural onStartServer(IJsonRpc& jsonServer, BredyHttpSrv::IHttpMapper &httpMapper) =0;

	///redefines exception handling for uncaught exception thrown from the RPC methods
	/** Function is called for every exception thrown from any RPC method. Server can
	 * define reaction for every exception
	 *
	 * @param json pointer to JSON factory which helps to build RpcError object
	 * @param e reference to exception. You have to use dynamic_cast to find type of the exception
	 * @return function have to return standard RpcError object that is emited as error object with
	 * two variables - status and statusMessage. However, RpcError object can carry any JSON object.
	 * For unknown exceptions, function should call original implementation, which can handle all
	 * other exceptions.ddddddddddddddddddddd
	 *
	 */
	virtual RpcError customRPCException(JSON::IFactory *json, const std::exception &e);


	virtual void onStopServer() {};
protected:
	///called when control message arrives at ServiceApp interface.
	/**
	 * @param command command read from command line
	 * @param args various arguments
	 * @param output reference or stderr. You should not to use standard stderr, it is probably disconnected. Message
	 *   written into this output will be transfered as output of caller.
	 * @return function return result of the call passed to the caller. Overrider  should call original method
	 *  for commands, which doesn't handle and carry return value.
	 */
    virtual integer onMessage(ConstStrA command, const Args & args, SeqFileOutput output);
    ///You can override this method to implement custom exception handler for unhandled exceptions in server threads
    virtual void onThreadException(const Exception &e) throw();
    ///You can override this method to implement custom exception handler for unhandled exceptions in master thread
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
	BredyHttpSrv::HttpServer::Config serverConfig;
	String clientPage;
	bool serverControl;
	bool started;
	String helpdir;
	String rpclogfile;
	StringA baseUrl;
	StringA serverIdent;
	FilePath appPath;
	JsonHttpServer *jsonrpcserver;
	lnatural exceptSumTm;
	StringA livelog;
	StringA livelog_realm;
	StringA livelog_userlist;
	String cfgPath;
	StringA usergroup;





	void readMainConfig(const IniConfig &cfg);
};

} /* namespace jsonsvc */
#endif /* JSONRPCSERVER_ABSTRACTJSONRPCSERVER_H_ */
