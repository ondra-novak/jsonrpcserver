/*
 * ilog.h
 *
 *  Created on: 29. 9. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONSERVER_ILOG_H_
#define JSONRPC_JSONSERVER_ILOG_H_
#include <lightspeed/base/interface.h>
#include "lightspeed/base/containers/constStr.h"
#include <lightspeed/utils/json/json.h>

namespace BredyHttpSrv {
	class IHttpRequestInfo;
}

namespace jsonrpc {

///interface that helps to log method.
/**
 * Interface can be retrieve from IJsonRpc using getIfcPtr if target object supports logging
 * For example JsonRpcServer object supports standard loging into rpclogfile. Using
 * this interface is way how to log methods there;
 */
	class ILog: public LightSpeed::IInterface {
	public:
		///Log method call to rpclog
		/**
		 * @param invoker reference to request that invoked the call
		 * @param methodName method name
		 * @param params params of the call
		 * @param context context of the call (optional)
		 * @param logOutput output of the call (optional)
		 */
		virtual void logMethod(BredyHttpSrv::IHttpRequestInfo &invoker, LightSpeed::ConstStrA methodName, const LightSpeed::JSON::ConstValue &params, const LightSpeed::JSON::ConstValue &context, const LightSpeed::JSON::ConstValue &logOutput) = 0;
		///More general loging of the method, especially when they came from a different source, that http.
		/**
		 * @param source source. In original interface there is IP address of the caller. However, you can now specify different source.
		 * @param methodName name of the method
		 * @param params params or the call
		 * @param context context of the call (optional)
		 * @param logOutput output of the call (optional)
		 */
		virtual void logMethod(LightSpeed::ConstStrA source, LightSpeed::ConstStrA methodName, const LightSpeed::JSON::ConstValue &params, const LightSpeed::JSON::ConstValue &context, const LightSpeed::JSON::ConstValue &logOutput) = 0;
	};


}



#endif /* JSONRPC_JSONSERVER_ILOG_H_ */
