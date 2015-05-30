#pragma once
#include "jsonRpc.h"
#include "lightspeed/utils/configParser.h"
#include "lightspeed/utils/FilePath.h"
#include "../httpserver/statBuffer.h"
#include "lightspeed/base/streams/fileiobuff.h"
#include "lightspeed/utils/json/jsondefs.h"


namespace jsonsrv {

	class JsonRpcServer: public JsonRpc, public IJsonRpcLogObject {
	public:
		JsonRpcServer();

		///Initialize JSONRPC interface using default config
		void init(const IniConfig &cfg);

		///Initialize JSONRPC interface using a section in the config
		void init(const IniConfig::Section sect);

		///Initialize JSONRPC interface using values
		/**
		 * @param rpclogfilePath path where rpc-log will be put. Empty to disable log
		 * @param helpdirPath path where help to methods resides. Empty - no help
		 * @param clientPagePath path to web-based RPC client. Empty - no client
		 */
		void init(const FilePath &rpclogfilePath,
						const FilePath &helpdirPath,
						const FilePath &clientPagePath);

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
		 * @param context context of the call
		 * @param logOutput output of the call
		 */
		void logMethod(IHttpRequest &invoker, ConstStrA methodName, JSON::INode *params, JSON::INode *context, JSON::INode *logOutput);
		///Opens, reopens the log
		void openLog();


		template<typename T>
		static JSON::PNode avgField(const StatBuffer<T> &b, JSON::IFactory &f, natural cnt);


		template<typename T>
		static JSON::PNode minField(const StatBuffer<T> &b, JSON::IFactory &f, natural cnt);


		template<typename T>
		static JSON::PNode maxField(const StatBuffer<T> &b, JSON::IFactory &f, natural cnt);


		template<typename T>
		static JSON::PNode statFields(const StatBuffer<T> &b, JSON::IFactory &f);

		JSON::PNode rpcHttpStatHandler(RpcRequest *rq);
	protected:
		AllocPointer<SeqFileOutBuff<> >logfile;
		FastLock lock;
		String logFileName;		

		JSON::Factory_t::Stream_t strparams,strcontext,stroutput;

	};


}