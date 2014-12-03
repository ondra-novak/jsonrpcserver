/*
 * ijsonrpc.h
 *
 *  Created on: Sep 24, 2012
 *      Author: ondra
 */

#ifndef JSONRPC_IJSONRPC_H_
#define JSONRPC_IJSONRPC_H_
#include "rpchandler.h"


namespace jsonsrv {

	class IJsonRpcLogObject {
	public:
		virtual void logMethod(IHttpRequest &invoker, ConstStrA methodName, JSON::INode *params, JSON::INode *context, JSON::INode *output) = 0;
		virtual ~IJsonRpcLogObject() {}
	};


	class IJsonRpc {
	public:
		///Registers RPC method
		/**
		 * @param methodName Method name, optionally with arguments. There can be only the name in case, that
		 *   handler will check arguments by own. You can add double collon ':' after method followed by list
		 *   of argument types, where every argument is specified by one character. Types can be: 'n' - number (int
		 *   or double), 's' - string, 'a' - array, 'c' or 'o' - object, 'b' - boolean and 'x' - null. In case that
		 *   arguments are specified, you can register more handlers for one method with different argument list -
		 *   overloading. Server will check arguments and call appropriate handler.
		 * @param method Handler reference
		 * @param help Help for this method. This is optional and if is not specified, method has no help. This
		 *  doesn't prevent to add help using help files. In case of overloading, help is associated with
		 *  the method by name, so only first help is used, others are ignored.
		 *
		 */
	    virtual void registerMethod(ConstStrA methodName, const IRpcCall & method, ConstStrA help = ConstStrA()) = 0;
	    ///Erases method
	    virtual void eraseMethod(ConstStrA methodName) = 0;
	    ///Sets log handler
	    /**
	     * @param logObject monitors RPC callings for logging
	     */
	    virtual void setLogObject(IJsonRpcLogObject *logObject) = 0;
	    ///Registers global method
	    /** Global method handler is called for every method, after function is executed. Global handler
	     * can modify result or add some other informations to the context.
	     * @param methodUID method unique identifier. It is used to identify method for deleting on replacing. If
	     * 	there is method with same name, it is replaced. MethodUID is not used as methodName, global handlers are
	     * 	called everytime
	     *
	     * @param method method handler
	     */
	    virtual void registerGlobalHandler(ConstStrA methodUID, const IRpcCall & method) = 0;
	    ///Erases global handler
	    virtual void eraseGlobalHandler(ConstStrA methodUID) = 0;

	    ///Registers obsolete method
	    /**
	     * Method with this name is not appear in list of methods and returns error 426
	     * @param methodName
	     */
	    virtual void registerMethodObsolete(ConstStrA methodName) = 0;

	    ///registers global statistics handler
	    /**
	     * @param handlerName name of handler - statistics will appear under this name
	     * @param method method which returns statistics. Function receives arguments passed through function server.stats
	     */
	    virtual void registerStatHandler(ConstStrA handlerName, const IRpcCall & method) = 0;
	    ///removes stat handler
	    virtual void eraseStatHandler(ConstStrA handlerName) = 0;

	    ///Specifies maximum size of JSON request.
	    /** You can limit size of request in bytes, Bigger requests are rejected by http server
	     *
	     * @param bytes maximal size of request in bytes. Set to naturalNull to allow unlimited
	     * size of the request. This is not recomended however. Always ensure, that your application
	     * set this limit. Default value is 4MB.
	     */
	    virtual void setRequestMaxSize(natural bytes) = 0;

	    virtual ~IJsonRpc() {}

	};


}


#endif /* JSONRPC_IJSONRPC_H_ */
