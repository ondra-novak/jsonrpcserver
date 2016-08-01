#pragma once
#include "jsonRpc.h"
#include "lightspeed/utils/configParser.h"
#include "lightspeed/utils/FilePath.h"
#include "../httpserver/statBuffer.h"
#include "lightspeed/base/streams/fileiobuff.h"
#include "lightspeed/utils/json/jsondefs.h"


namespace jsonsrv {

	/// JSON Server


	class JsonRpcServer: public JsonRpc, public IJsonRpcLogObject {
	public:
		JsonRpcServer();

		///Initialize JSONRPC interface using default configuration
		void init(const IniConfig &cfg);

		///Initialize JSONRPC interface using a section in the configuration
		/** Configuration:
		 *
		 *  @b clientPage  - path to web page which is returned for GET request at RPC uri. If not defined, GET returns 404
		 *
		 *  @b helpDir - path to directory with help files. If not defined, no help is available
		 *
		 *  @b rpcLog - path to file, where methods will be logged. If not defined, logging is disabled
		 *
		 *  @b allowCORSOrigins - list of allowed origins separated by space. Specifying this option in a configuration file
		 *                         also enables CORS handling.
		 *
		 *  @b developMode - set to true to expose development methods (default : false)
		 *
		 *  @b multicall - set to false to disable multicall (default : true)
		 *
		 *  @b listMethods - set to false to disable Server.listMethods (default : true)
		 *
		 *  @b enableStats - set to false to disable Server.stats (default : true)
		 *
		 * @param sect
		 */
		void init(const IniConfig::Section sect);

		struct Options {
			FilePath logFile;
			FilePath helpDir;
			FilePath clientPage;
			Optional<StringA> corsOrigin;
			bool developMode;
			bool enableMulticall;
			bool enableListMethods;
			bool enableStats;

			Options() :developMode(false), enableMulticall(true), enableListMethods(true),enableStats(true) {}
		};

		///Initialize JSONRPC interface using values
		/**
		 */
		void init(const Options &options);

		///Perform logRotate of the rpclog
		void logRotate() {openLog();}

		virtual void registerMethod(ConstStrA methodName, const IRpcCall & method, ConstStrA help = ConstStrA());

		virtual void registerGlobalHandler(ConstStrA methodName, const IRpcCall & method);

		virtual void eraseMethod(ConstStrA methodName);

		virtual void eraseGlobalHandler(ConstStrA methodName);

		virtual void registerStatHandler(ConstStrA handlerName, const IRpcCall & method);
		///removes stat handler
		virtual void eraseStatHandler(ConstStrA handlerName);

		virtual RpcError onException(JSON::IFactory *json, const std::exception &e);

	protected:
		///Log method call to rpclog
		/**
		 * @param invoker reference to request that invoked the call
		 * @param methodName method name
		 * @param params params of the call
		 * @param context context of the call (optional)
		 * @param logOutput output of the call (optional)
		 */
		virtual void logMethod(IHttpRequest &invoker, ConstStrA methodName, const JSON::ConstValue &params, const JSON::ConstValue &context, const JSON::ConstValue &logOutput);
		///More general loging of the method, especially when they came from a different source, that http.
		/**
		 * @param source source. In original interface there is IP address of the caller. However, you can now specify different source.
		 * @param methodName name of the method
		 * @param params params or the call
		 * @param context context of the call (optional)
		 * @param logOutput output of the call (optional)
		 */
		virtual void logMethod(ConstStrA source, ConstStrA methodName, const JSON::ConstValue &params, const JSON::ConstValue &context, const JSON::ConstValue &logOutput);
		///Opens, reopens the log
		void openLog();
		JSON::PNode rpcHttpStatHandler(RpcRequest *rq);
	protected:
		AllocPointer<SeqFileOutBuff<> >logfile;
		FastLock lock;
		String logFileName;
		JSON::Value nullVal;

		struct LogBuffers {
			JSON::Factory::Stream_t strparams,strcontext,stroutput;

		};
		ThreadVarInitDefault<LogBuffers> logBuffers;
		atomic logRotateCounter;

		JSON::ConstValue nullV;
	};


}
