/*
 * ServerMain.cpp
 *
 *  Created on: Sep 1, 2012
 *      Author: ondra
 */

#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/base/framework/cmdLineIterator.h"
#include "lightspeed/utils/configParser.tcc"
#include "lightspeed/base/text/textstream.tcc"
#include "JSONRPCMain.h"
#include "lightspeed/base/debug/stdlogoutput.h"
#include "../httpserver/httpServer.h"
#include "jsonRpc.h"
#include "lightspeed/utils/json/jsondefs.h"
#include "lightspeed/utils/json/jsonfast.tcc"
#include "lightspeed/base/sync/synchronize.h"
#include "lightspeed/base/interface.tcc"
#include "lightspeed/base/debug/livelog.h"
#include <pwd.h>
#include <unistd.h>
#include <grp.h>




namespace jsonsrv {

using namespace LightSpeed;
using namespace BredyHttpSrv;

/*
static inline StdLogOutput &getStdLog() {
	return Singleton<StdLogOutput>::getInstance();
}
*/



static void unknownSwitchError(SeqFileOutput &conerr, bool longSwitch, ConstStrW value) {
	const char *text = "Unknown switch '%1%%2'\r\n";
	PrintTextW print(conerr);
	if (longSwitch) print(text) << "--" << value;
	else print(text) << "-" << value[0];
}

static void unknownParameter(SeqFileOutput &conerr, ConstStrW value) {
	const char *text = "Unknown parameter '%1'\r\n";
	PrintTextW print(conerr);
	print(text) <<  value;
}


integer AbstractJSONRPCServer::initService(const Args& args, SeqFileOutput serr) {

	enableRestartOnError(5);
	FilePath appPathname(getAppPathname());
	appPath = appPathname/nil;

	PrintTextW print(serr);
	print("Starting JSONRPC server '%1'\n") << appPathname.getTitle();
	cfgPath.clear();
	CmdLineIterator iter(args,0);
	while (iter.hasItems()) {
		CmdLineItem itm = iter.getNext();
		if (itm.itemType == CmdLineItem::shortSwitch) {
			switch (itm.value[0]) {
				case 'f': cfgPath = iter.getNextText();break;
				default: unknownSwitchError(serr,false,itm.value);return 1;
			}
		} else if (itm.itemType == CmdLineItem::longSwitch) {
			unknownSwitchError(serr,true,itm.value);
			return 1;
		} else {
			unknownParameter(serr,itm.value);
			return 1;
		}
	}
	if (cfgPath.empty()) {
		FilePath cfgP = appPath/parent/ConstStrA("conf")/String(appPathname.getTitle()+ConstStrW(L".conf"));
		cfgPath = cfgP;
	}

	IniConfig cfg(ConstStrW(cfgPath),OpenFlags::shareRead | OpenFlags::shareDelete);


	readMainConfig(cfg);

	//probe TCP port
	LogObject(THISLOCATION).progress("----------------- Initializing service ---------------------");
	LogObject(THISLOCATION).note("Probe port: %1") << port;
	NetworkStreamSource src(port,1,1,1,false);

	for (PortList::Iterator iter = otherPorts.getFwIter(); iter.hasItems();) {
		natural port = iter.getNext();
		LogObject(THISLOCATION).note("Probe other port: %1") << port;
		NetworkStreamSource src(port,1,1,1,false);
	}


	natural isr = onInitServer(args,serr,cfg);
	if (isr != 0) return isr
			;
	return ServiceApp::initService(args,serr);
}

class JsonHttpServer: public JsonRpc, public BredyHttpSrv::HttpServer, public IJsonRpcLogObject {
public:
	JsonHttpServer(AbstractJSONRPCServer &owner, StringA baseUrl,
			StringA serverIdent, const Config &cfg, String logFileName)
		:BredyHttpSrv::HttpServer(baseUrl, serverIdent,cfg),owner(owner),logFileName(logFileName)
	{
			addSite("/RPC",this);
			setLogObject(this);
			addLiveLog("/livelog");
			registerStatHandler("server",RpcCall::create(this,&JsonHttpServer::rpcHttpStatHandler));
	}
	void logMethod(IHttpRequest &invoker, ConstStrA methodName, JSON::INode *params, JSON::INode *context, JSON::INode *logOutput) {
		if (logfile == nil) return;
		LogObject lg(THISLOCATION);
		IHttpPeerInfo &pinfo = invoker.getIfc<IHttpPeerInfo>();
		ConstStrA peerAddr = pinfo.getPeerAddrStr();
		natural q = peerAddr.findLast(':');
		if (q != naturalNull) peerAddr = peerAddr.head(q);
		ConstStrA paramsStr;
		strparams.clear();
		strcontext.clear();
		stroutput.clear();
		if (params == 0) params = JSON::getNullNode();
		if (context == 0) context = JSON::getNullNode();
		if (logOutput == 0) logOutput = JSON::getNullNode();
		JSON::serialize(params,strparams,false);
		JSON::serialize(context,strcontext,false);
		JSON::serialize(logOutput,stroutput,false);;
		ConstStrA resparamstr(strparams.getArray());
		ConstStrA rescontextptr(strcontext.getArray());
		ConstStrA resoutputptr(stroutput.getArray());
		Synchronized<FastLock> _(lock);
		PrintTextA pr(*logfile);
		AbstractLogProvider::LogTimestamp tms;
		pr("%{04}1/%{02}2/%{02}3 %{02}4:%{02}5:%{02}6 - [\"%7\",\"%8\",%9,%10,%11]\n")
			<< tms.year << tms.month << tms.day
			<< tms.hour << tms.min << tms.sec
			<< peerAddr << methodName << resparamstr << rescontextptr << resoutputptr;
	}

	void openLog() {
		if (logFileName.empty()) return;
		Synchronized<FastLock> _(lock);
		try {
			logfile = new SeqFileOutBuff<>(logFileName,OpenFlags::append|OpenFlags::create);
		} catch (...) {

		}
	}

    virtual void registerMethod(ConstStrA methodName, const IRpcCall & method, ConstStrA help) {
    	LogObject lg(THISLOCATION);
    	JsonRpc::registerMethod(methodName,method,help);
    	lg.progress("Server: Method registered: %1") << methodName;
    }

    virtual void registerGlobalHandler(ConstStrA methodName, const IRpcCall & method) {
    	LogObject lg(THISLOCATION);
    	JsonRpc::registerGlobalHandler(methodName,method);
    	lg.progress("Server: Global handler registered: %1") << methodName;
    }

    virtual void eraseMethod(ConstStrA methodName) {
    	LogObject lg(THISLOCATION);
    	JsonRpc::eraseMethod(methodName);
    	lg.progress("Server: Method removed: %1") << methodName;
    }

    virtual void eraseGlobalHandler(ConstStrA methodName) {
    	LogObject lg(THISLOCATION);
    	JsonRpc::eraseGlobalHandler(methodName);
    	lg.progress("Server: Global handler removed: %1") << methodName;
    }

	virtual void addSite(ConstStrA path, IHttpHandler *handler) {
    	LogObject lg(THISLOCATION);
    	HttpServer::addSite(path,handler);
    	lg.progress("Server: HTTP site added at path: %1") << path;

	}
	virtual void removeSite(ConstStrA path) {
    	LogObject lg(THISLOCATION);
    	HttpServer::removeSite(path);
    	lg.progress("Server: HTTP site removed at path: %1") << path;

	}
    virtual void registerStatHandler(ConstStrA handlerName, const IRpcCall & method) {
    	LogObject lg(THISLOCATION);
    	JsonRpc::registerStatHandler(handlerName,method);
    	lg.progress("Server: Added statistics %1") << handlerName;

    }
    ///removes stat handler
    virtual void eraseStatHandler(ConstStrA handlerName) {
    	LogObject lg(THISLOCATION);
    	JsonRpc::eraseStatHandler(handlerName);
    	lg.progress("Server: Removed statistics %1") << handlerName;

    }

    virtual RpcError onException(JSON::IFactory *json, const std::exception &e) {
    	return owner.customRPCException(json,e);
    }

    template<typename T>
    static JSON::PNode avgField(const StatBuffer<T> &b, JSON::IFactory &f, natural cnt) {
    	T c = b.getAvg(cnt);
    	if (c.isDefined()) return f.newValue(c.getValue());
    	else return f.newNullNode();
    }


    template<typename T>
    static JSON::PNode minField(const StatBuffer<T> &b, JSON::IFactory &f, natural cnt) {
    	T c = b.getMin(cnt);
    	if (c.isDefined()) return f.newValue(c.getValue());
    	else return f.newNullNode();
    }


    template<typename T>
    static JSON::PNode maxField(const StatBuffer<T> &b, JSON::IFactory &f, natural cnt) {
    	T c = b.getMax(cnt);
    	if (c.isDefined()) return f.newValue(c.getValue());
    	else return f.newNullNode();
    }


    template<typename T>
    static JSON::PNode statFields(const StatBuffer<T> &b, JSON::IFactory &f) {
    	JSON::PNode a = f.newClass();
    	a->add("avg006",avgField(b,f,6));
    	a->add("avg030",avgField(b,f,30));
    	a->add("avg060",avgField(b,f,60));
    	a->add("avg300",avgField(b,f,300));
    	a->add("min006",minField(b,f,6));
    	a->add("min030",minField(b,f,30));
    	a->add("min060",minField(b,f,60));
    	a->add("min300",minField(b,f,300));
    	a->add("max006",maxField(b,f,6));
    	a->add("max030",maxField(b,f,30));
    	a->add("max060",maxField(b,f,60));
    	a->add("max300",maxField(b,f,300));
    	return a;
    }

    JSON::PNode rpcHttpStatHandler(RpcRequest *rq) {
    	JSON::IFactory &f = *(rq->jsonFactory);
    	const HttpStats &st = getStats();
    	JSON::PNode out = rq->jsonFactory->newClass();
    	out->add("request", statFields(st.requests,f))
    		->add("threads", statFields(st.threads,f))
    		->add("threadsIdle", statFields(st.idleThreads,f))
    		->add("connections", statFields(st.connections,f))
    		->add("latency", statFields(st.latency,f))
    		->add("worktime", statFields(st.worktime,f));
    	return out;

    }
protected:
	AbstractJSONRPCServer &owner;
	AllocPointer<SeqFileOutBuff<> >logfile;
	FastLock lock;
	String logFileName;

	JSON::Factory_t::Stream_t strparams,strcontext,stroutput;

};


integer AbstractJSONRPCServer::startService() {

	started = false;
	LogObject lg(THISLOCATION);
	lg.note("----------------- Entering service ---------------------");
	lg.note("Initializing server: %1") << serverIdent;
	lg.note("Configuration: port=%1, maxThreads=%2, maxBusyThreads=%3") << port << serverConfig.maxThreads << serverConfig.maxBusyThreads;
	lg.note("Configuration: newThreadTimeout=%1, threadIdleTimeout = %2") << serverConfig.newThreadTimeout << serverConfig.threadIdleTimeout;
	lg.note("Configuration: serverControl=%1, clientAppPath=%2") << serverControl << clientPage;

	JsonHttpServer srv(*this,baseUrl,serverIdent,serverConfig,rpclogfile);
	srv.openLog();
	jsonrpcserver = &srv;
	FilePath p(getAppPathname());

	if (!clientPage.empty()) {
		FilePath client = p / clientPage;
		srv.setClientPage(client);
	}
	if (!helpdir.empty()) {
		try {
			FilePath help = p / helpdir;
	    		PDirectoryIterator iter = IFileIOServices::getIOServices().openDirectory(help);
			while (iter->getNext()) {
			    try {
				    srv.loadHelp(iter->getFullPath());
			    } catch (Exception &e) {
				lg.warning("Skipping help file: %1 error %2") << iter->entryName << e.getMessage();
			    }
	    		}
	    	} catch (Exception &e) {
	    	    lg.warning("Unable to open help folder: %1") << e.getMessage();
	    	}
	}

	srv.registerServerMethods(serverControl);

	if (!livelog.empty()) {
		if (livelog_userlist.empty()) {
			srv.addLiveLog(livelog);
		} else {
			ConstStrA realm;
			if (livelog_realm.empty()) realm="livelog";
			else realm = livelog_realm;
			srv.addLiveLog(livelog,realm,livelog_userlist);
		}

	}

	lg.progress("Starting server on port: %1 (master)") << port;
	srv.start(port);
	lg.progress("Server started");
	started = true;
	for (PortList::Iterator iter = otherPorts.getFwIter();iter.hasItems();) {
		natural port = iter.getNext();
		natural k = srv.addPort(port);
		lg.progress("Added port (id:%2) : %1") << port << k;
	}

	if (!usergroup.empty()) {
		setUserGroup(usergroup);
	} else if (getuid() == 0) {
		lg.warning("STARTING UNDER root ACCOUNT! Consider to specify user and group in server.setusergroup config options");
	}

	natural res = onStartServer(srv,srv);
	if (res != 0) {
		lg.fatal("onStartServer returned nonzero result: %1") << res;
	}

	res = ServiceApp::startService();

	LogObject(THISLOCATION).progress("Exiting server (exit code: %1)") << res;
	srv.stop();
	onStopServer();
	LogObject(THISLOCATION).progress("----------------- Done ---------------------");
	LogObject(THISLOCATION).progress("");
	jsonrpcserver = 0;
	return res;

}
integer AbstractJSONRPCServer::onMessage(ConstStrA command, const Args& args,
		SeqFileOutput output) {
	LogObject lg(THISLOCATION);
	lg.debug("Command: %1 %2 %3 %4 %5 %6 %7")
			<< command
			<< (args.length() > 0?args[0]:L"")
			<< (args.length() > 1?args[1]:L"")
			<< (args.length() > 2?args[2]:L"")
			<< (args.length() > 3?args[3]:L"")
			<< (args.length() > 4?args[4]:L"")
			<< (args.length() > 5?L"...":L"");
	if (command == "logrotate") {
		lg.note("End of log (logrotate)");
		lg.getOutput().getIfc<StdLogOutput>().logRotate();
		jsonrpcserver->openLog();
		return 0;
	} else {
		return ServiceApp::onMessage(command,args,output);

	}

}


ILogOutput& AbstractJSONRPCServer::logOutputSingleton() {
	return Singleton<LiveLog>::getInstance();
}


void AbstractJSONRPCServer::onThreadException(const Exception& e) throw() {
	try {
		LogObject(THISLOCATION).fatal("Thread exception: %1") << e.what();
		time_t t;
		time(&t);
		lnatural x = t;
		lnatural s = exceptSumTm / 100;
		exceptSumTm = exceptSumTm - s + x;
		if (x - s < 3) {
			errorRestart();
		}
	} catch (...) {

	}
}

void AbstractJSONRPCServer::onException(const Exception& e) {
	LogObject(THISLOCATION).fatal("Exception: %1") << e.what();
}


void AbstractJSONRPCServer::readMainConfig(const IniConfig& cfg) {
	LogObject lg(THISLOCATION);
	FilePath p(getAppPathname());
	IniConfig::Section sect = cfg.openSection("server");
	String logfile;
	natural tmp;
	StringA loglevel = "all";
	serverControl = true;
	sect.required(port, "port",0);
	for (int i = 1; sect.get(tmp,"port",i); i++) {
		otherPorts.add(tmp);
	}
	serverConfig.newThreadTimeout = 0;
	serverConfig.threadIdleTimeout = 120000;
	sect.required(serverConfig.maxThreads,"threads");
	sect.required(serverIdent,"serverIdent");
	sect.required(baseUrl,"baseUrl");
	sect.required(serverConfig.maxBusyThreads, "busyThreads");
	sect.get(serverConfig.newThreadTimeout, "newThreadTimeout");
	sect.get(serverConfig.threadIdleTimeout, "threadIdleTimeout");
	sect.get(clientPage,"clientPage");
	sect.get(serverControl,"serverControl");
	sect.get(helpdir,"helpDir");
	sect.required(logfile,"log");
	sect.get(loglevel,"logLevel");
	sect.get(rpclogfile,"rpclog");
	sect.get(livelog,"livelog");
	sect.get(usergroup,"setusergroup");
	if (!livelog.empty()) {
		DbgLog::setLogProvider(&logOutputSingleton);
		sect.get(livelog_realm,"livelog.realm");
		sect.get(livelog_userlist,"livelog.auth");
	}
	StdLogOutput *stdLog = DbgLog::getStdLog();
	if (stdLog) {
		stdLog->setFile(FilePath(p/logfile));
		stdLog->enableStderr(false);
	}
	DbgLog::setLogLevel(loglevel);
	if (!rpclogfile.empty()) rpclogfile = FilePath(p/rpclogfile);
}

void AbstractJSONRPCServer::setUserGroup(ConstStrA usergroup) {
	LogObject lg(THISLOCATION);
	ConstStrA::SplitIterator splt = usergroup.split(':');
	StringA tmp;
	ConstStrA username = splt.getNext();
	ConstStrA grpname = splt.hasItems()?splt.getNext():ConstStrA();

	if (!grpname.empty()) {
		struct group *info = getgrnam(cStr(username,tmp));
		if (info == 0) {
			int er = errno;
			throw ErrNoWithDescException(THISLOCATION,er,
					String(ConstStrA("Unknown group: ") + grpname));
		}
		lg.progress("Changing current group to: %1 (%2)") << info->gr_gid << grpname;
		if (setregid(info->gr_gid,info->gr_gid)) {
			int er = errno;
			throw ErrNoWithDescException(THISLOCATION,er,
					String(ConstStrA("Can't switch to group: ") + username));
		}
	}

	if (!username.empty()) {
		struct passwd *info = getpwnam(cStr(username,tmp));
		if (info == 0) {
			int er = errno;
			throw ErrNoWithDescException(THISLOCATION,er,
					String(ConstStrA("Unknown user: ") + username));
		}
		lg.progress("Changing current user to: %1 (%2)") << info->pw_uid << username;
		if (setreuid(info->pw_uid,info->pw_uid)) {
			int er = errno;
			throw ErrNoWithDescException(THISLOCATION,er,
					String(ConstStrA("Can't switch to user: ") + username));
		}
	}

}

RpcError AbstractJSONRPCServer::customRPCException(JSON::IFactory *json, const std::exception& e) {
	return jsonrpcserver->JsonRpc::onException(json,e);
}

		}

