/*
 * serverMain.cpp
 *
 *  Created on: Apr 15, 2015
 *      Author: ondra
 */

#include "lightspeed/base/platform.h"
#ifdef LIGHTSPEED_PLATFORM_LINUX
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#endif


#include "lightspeed/base/framework/cmdLineIterator.h"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/base/debug/stdlogoutput.h"
#include "lightspeed/base/debug/livelog.h"
#include "serverMain.h"


namespace BredyHttpSrv {

using namespace LightSpeed;

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

integer AbstractServerMain::initService(const Args& args, SeqFileOutput serr) {

	enableRestartOnError(5);
	FilePath appPathname(getAppPathname());
	appPath = appPathname/nil;

	PrintTextW print(serr);
	print("Starting HTTP server '%1'\n") << appPathname.getTitle();
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

	LS_LOGOBJ(lg);

	//probe TCP port
	lg.progress("----------------- Initializing the server ---------------------");
	lg.note("Probe port: %1") << port;
	NetworkStreamSource src(port,1,1,1,false);

	for (PortList::Iterator iter = otherPorts.getFwIter(); iter.hasItems();) {
		natural port = iter.getNext();
		lg.note("Probe other port: %1") << port;
		NetworkStreamSource src(port,1,1,1,false);
	}


	natural isr = onInitServer(args,serr,cfg);
	if (isr != 0) return isr;
	return ServiceApp::initService(args,serr);
}

integer AbstractServerMain::startService() {

	started = false;
	LogObject lg(THISLOCATION);
	lg.note("----------------- Entering service ---------------------");
	lg.note("Initializing server: %1") << serverIdent;
	lg.note("Configuration: port=%1, maxThreads=%2, maxBusyThreads=%3") << port << serverConfig.maxThreads << serverConfig.maxBusyThreads;
	lg.note("Configuration: newThreadTimeout=%1, threadIdleTimeout = %2") << serverConfig.newThreadTimeout << serverConfig.threadIdleTimeout;

	HttpServer srv(baseUrl,serverIdent, serverConfig);
	natural res = onStartServer(srv);
	if (res != 0) {
		lg.fatal("onStartServer returned nonzero result: %1 - exiting") << res;
	}

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

#ifdef LIGHTSPEED_PLATFORM_LINUX
	if (!usergroup.empty()) {
		setUserGroup(usergroup);
	} else if (getuid() == 0) {
		lg.warning("STARTING UNDER root ACCOUNT! Consider to specify user and group in server.setusergroup config options");
	}
#endif

	onServerReady(srv);

	res = ServiceApp::startService();

	lg.progress("Exiting server (exit code: %1)") << res;
	onStopServer(srv);
	srv.stop();
	onServerFinish(srv);
	lg.progress("----------------- Done ---------------------");
	lg.progress("");
	return res;

}
integer AbstractServerMain::onMessage(ConstStrA command, const Args& args,
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
		DbgLog::logRotate();
		return 0;
	} else {
		return ServiceApp::onMessage(command,args,output);

	}

}


ILogOutput& AbstractServerMain::logOutputSingleton() {
	return Singleton<LiveLog>::getInstance();
}


void AbstractServerMain::onThreadException(const Exception& e) throw() {
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

void AbstractServerMain::onException(const Exception& e) {
	LogObject(THISLOCATION).fatal("Exception: %1") << e.what();
}


void AbstractServerMain::readMainConfig(const IniConfig& cfg) {
	LogObject lg(THISLOCATION);
	FilePath p(getAppPathname());
	IniConfig::Section sect = cfg.openSection("server");
	String logfile;
	natural tmp;
	StringA loglevel = "all";
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
	sect.required(logfile,"log");
	sect.get(loglevel,"logLevel");
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
}

void AbstractServerMain::setUserGroup(ConstStrA usergroup) {
#ifdef LIGHTSPEED_PLATFORM_LINUX
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
#endif
}



}



