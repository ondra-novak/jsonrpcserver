#include "jsonRpcServer.h"
#include "lightspeed/base/framework/app.h"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/utils/json/jsonfast.tcc"
#include "lightspeed/base/debug/LogProvider.h"
#include "../httpserver/serverStats.h"
#include "../httpserver/httpServer.h"

namespace jsonsrv {




	JsonRpcServer::JsonRpcServer()
	{
		setLogObject(this);
		DbgLog::needRotateLogs(logRotateCounter);
	}

	void JsonRpcServer::init( const IniConfig &cfg )
	{
		IniConfig::Section sect = cfg.openSection("server");
		init(sect);
	}

	void JsonRpcServer::init( const IniConfig::Section sect )
	{
		StringA clientPage;
		StringA helpdir;
		StringA rpclogfile;


		FilePath p(App::current().getAppPathname());
		FilePath clientPagePath;
		FilePath helpdirPath;
		FilePath rpclogfilePath;

		sect.get(clientPage,"clientPage");
		if (!clientPage.empty()) {
			clientPagePath = p / clientPage;				
		}
		sect.get(helpdir,"helpDir");
		if (!helpdir.empty()) {
			helpdirPath = p / helpdir;
		}
		sect.get(rpclogfile,"rpcLog");
		if (!rpclogfile.empty()) {
			rpclogfilePath = p / rpclogfile;
		}

		init(rpclogfilePath, helpdirPath, clientPagePath);
	}

	void JsonRpcServer::init( const FilePath &rpclogfilePath, const FilePath &helpdirPath, const FilePath &clientPagePath )
	{
		LogObject lg(THISLOCATION);
		logFileName = rpclogfilePath;
		openLog();
		if (!helpdirPath.empty()) {
			try {
				PFolderIterator iter = IFileIOServices::getIOServices().openFolder(helpdirPath);
				while (iter->getNext()) {
					try {
						loadHelp(iter->getFullPath());
					} catch (Exception &e) {
						lg.warning("Skipping help file: %1 error %2") << iter->entryName << e.getMessage();
					}
				}
			} catch (Exception &e) {
				lg.warning("Unable to open help folder: %1") << e.getMessage();
			}
		}
		setClientPage(clientPagePath);
		registerServerMethods(true);
		registerStatHandler("server",RpcCall::create(this,&JsonRpcServer::rpcHttpStatHandler));
	}

	void JsonRpcServer::logMethod( IHttpRequest &invoker, ConstStrA methodName, JSON::INode *params, JSON::INode *context, JSON::INode *logOutput )
	{
		if (logfile == nil) return;
		if (logRotateCounter != DbgLog::rotateCounter) {
			if (DbgLog::needRotateLogs(logRotateCounter)) {
				logRotate();
			}
		}

		LogObject lg(THISLOCATION);
		IHttpPeerInfo &pinfo = invoker.getIfc<IHttpPeerInfo>();
		ConstStrA peerAddr = pinfo.getPeerRealAddr();
		ConstStrA paramsStr;
		LogBuffers &buffers = logBuffers[ITLSTable::getInstance()];
		buffers.strparams.clear();
		buffers.strcontext.clear();
		buffers.stroutput.clear();
		if (params == 0) params = JSON::getNullNode();
		if (context == 0) context = JSON::getNullNode();
		if (logOutput == 0) logOutput = JSON::getNullNode();
		JSON::serialize(params,buffers.strparams,false);
		JSON::serialize(context,buffers.strcontext,false);
		JSON::serialize(logOutput,buffers.stroutput,false);;
		ConstStrA resparamstr(buffers.strparams.getArray());
		ConstStrA rescontextptr(buffers.strcontext.getArray());
		ConstStrA resoutputptr(buffers.stroutput.getArray());
		PrintTextA pr(*logfile);
		AbstractLogProvider::LogTimestamp tms;

		Synchronized<FastLock> _(lock);

		pr("%{04}1/%{02}2/%{02}3 %{02}4:%{02}5:%{02}6 - [\"%7\",\"%8\",%9,%10,%11]\n")
			<< tms.year << tms.month << tms.day
			<< tms.hour << tms.min << tms.sec
			<< peerAddr << methodName << resparamstr << rescontextptr << resoutputptr;
		logfile->flush();
	}

	void JsonRpcServer::openLog()
	{
		if (logFileName.empty()) return;
		Synchronized<FastLock> _(lock);
		try {
			logfile = new SeqFileOutBuff<>(logFileName,OpenFlags::append|OpenFlags::create);
		} catch (...) {

		}
	}

	void JsonRpcServer::registerMethod( ConstStrA methodName, const IRpcCall & method, ConstStrA help )
	{
		LogObject lg(THISLOCATION);
		JsonRpc::registerMethod(methodName,method,help);
		lg.progress("Server: Method registered: %1") << methodName;
	}

	void JsonRpcServer::registerGlobalHandler( ConstStrA methodName, const IRpcCall & method )
	{
		LogObject lg(THISLOCATION);
		JsonRpc::registerGlobalHandler(methodName,method);
		lg.progress("Server: Global handler registered: %1") << methodName;
	}

	void JsonRpcServer::eraseMethod( ConstStrA methodName )
	{
		LogObject lg(THISLOCATION);
		JsonRpc::eraseMethod(methodName);
		lg.progress("Server: Method removed: %1") << methodName;
	}

	void JsonRpcServer::eraseGlobalHandler( ConstStrA methodName )
	{
		LogObject lg(THISLOCATION);
		JsonRpc::eraseGlobalHandler(methodName);
		lg.progress("Server: Global handler removed: %1") << methodName;
	}

	void JsonRpcServer::registerStatHandler( ConstStrA handlerName, const IRpcCall & method )
	{
		LogObject lg(THISLOCATION);
		JsonRpc::registerStatHandler(handlerName,method);
		lg.progress("Server: Added statistics %1") << handlerName;
	}

	void JsonRpcServer::eraseStatHandler( ConstStrA handlerName )
	{
		LogObject lg(THISLOCATION);
		JsonRpc::eraseStatHandler(handlerName);
		lg.progress("Server: Removed statistics %1") << handlerName;
	}

	jsonsrv::RpcError JsonRpcServer::onException( JSON::IFactory *json, const std::exception &e )
	{
		return JsonRpc::onException(json,e);
	}

	template<typename T>
	JSON::PNode JsonRpcServer::avgField( const StatBuffer<T> &b, JSON::IFactory &f, natural cnt )
	{
		T c = b.getAvg(cnt);
		if (c.isDefined()) return f.newValue(c.getValue());
		else return f.newNullNode();
	}

	template<typename T>
	JSON::PNode JsonRpcServer::minField( const StatBuffer<T> &b, JSON::IFactory &f, natural cnt )
	{
		T c = b.getMin(cnt);
		if (c.isDefined()) return f.newValue(c.getValue());
		else return f.newNullNode();
	}

	template<typename T>
	JSON::PNode JsonRpcServer::maxField( const StatBuffer<T> &b, JSON::IFactory &f, natural cnt )
	{
		T c = b.getMax(cnt);
		if (c.isDefined()) return f.newValue(c.getValue());
		else return f.newNullNode();
	}

	template<typename T>
	JSON::PNode JsonRpcServer::statFields( const StatBuffer<T> &b, JSON::IFactory &f )
	{
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

	JSON::PNode JsonRpcServer::rpcHttpStatHandler( RpcRequest *rq )
	{
		JSON::IFactory &f = *(rq->jsonFactory);
		const HttpServer &server = rq->httpRequest->getIfc<HttpServer>();
		const HttpStats &st = server.getStats();
		JSON::PNode out = rq->jsonFactory->newClass();
		out->add("request", statFields(st.requests,f))
			->add("threads", statFields(st.threads,f))
			->add("threadsIdle", statFields(st.idleThreads,f))
			->add("connections", statFields(st.connections,f))
			->add("latency", statFields(st.latency,f))
			->add("worktime", statFields(st.worktime,f));
		return out;
	}

}
