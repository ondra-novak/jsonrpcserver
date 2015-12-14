/*
 * ijsonrpcver.h
 *
 *  Created on: Dec 14, 2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_BREDY_VERSIONED_IJSONRPCVER_H_
#define JSONRPCSERVER_BREDY_VERSIONED_IJSONRPCVER_H_
#include <lightspeed/base/interface.h>

#include "ijsonrpc.h"
#include "rpchandler.h"


using LightSpeed::IInterface;


namespace jsonsrv {


	class IJsonRpcVer: public IJsonRpc{
	public:

		///Registers method on versioned API
		/**
		 * @param ver number of the version where method is registered. Method is visible at this
		 * version and newer unless it is overrided. Method is not visible at version below this
		 * version unless older one is defined
		 * @param method name of the method. If you specify method name with arguments, it is
		 *   tracked as separate method without connection to other methods with the same name
		 *   but a different arguments
		 * @param handler reference to the handler definition. Note that IRpcCall is clonable, function
		 * will create clone of the object for store definition internally,
		 *
		 */
		virtual void registerMethod(natural ver, ConstStrA method, const IRpcCall & handler) = 0;
		///Removes the methods from the api
		/**
		 * @param ver number specified version where method was specified. Alternative you can specify
		 *   any following version however in this case, method is not removed from the previous versions
		 * @param method name of the method (including arguments)
		 *
		 * If the argument 'ver' matches the version of the method, it is removed complete. However,
		 * if the argument 'ver' is above the version of the method, function removes the method for
		 * specified version and any future version, but previous versions are still available
		 */
		virtual void removeMethod(natural ver, ConstStrA method) = 0;

		using IJsonRpc::callMethod;
		///Calls other method
		/**
		 *
		 * @param httpRequest http request to read various HTTP states
		 * @param version version of the method (and this version is reported to the method)
		 * @param methodName name of the method (without arguments)
		 * @param args arguments
		 * @param context context JSON object
		 * @param id JSON identifier of the message
		 * @return Object contains return result
		 */
	    virtual CallResult callMethod(IHttpRequest *httpRequest, natural version, ConstStrA methodName, JSON::INode *args, JSON::INode *context, JSON::INode *id) = 0;

	    ///Sets help to the function
	    /**
	     * @param method method name without arguments. Methods shares the same help message
	     * @param text text of the message
	     */
	    virtual void setHelp(ConstStrA method, ConstStrA text) = 0;
	    ///Removes help for the method
	    virtual void unsetHelp(ConstStrA method) = 0;
	    ///Load help from the stream
	    virtual void loadHelp(SeqFileInput &stream) = 0;


	    using IJsonRpc::registerMethod;

	};


}



#endif /* JSONRPCSERVER_BREDY_VERSIONED_IJSONRPCVER_H_ */
