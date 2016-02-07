/*
 * ijsonrpc.h
 *
 *  Created on: Sep 24, 2012
 *      Author: ondra
 */

#ifndef JSONRPC_IJSONRPC_H_
#define JSONRPC_IJSONRPC_H_
#include "lightspeed/base/containers/optional.h"
#include "rpchandler.h"


namespace jsonsrv {

///interface that helps to log method.
/**
 * Interface can be retrieve from IJsonRpc using getIfcPtr if target object supports logging
 * For example JsonRpcServer object supports standard loging into rpclogfile. Using
 * this interface is way how to log methods there
 */
	class IJsonRpcLogObject: public IInterface {
	public:
		virtual void logMethod(IHttpRequest &invoker, ConstStrA methodName, JSON::INode *params, JSON::INode *context, JSON::INode *output) = 0;
	};


	class IJsonRpc: public IInterface {
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

	    struct CallResult {
	    	///Result of the call
	    	JSON::PNode result;
	    	///Error of the call (result is "null")
	    	JSON::PNode error;
	    	///Id returned to the caller
	    	JSON::PNode id;
	    	///New context, if changed, or NULL (no change)
	    	JSON::PNode newContext;
	    	///output that should be logged out - can be different than result (default is "null")
	    	JSON::PNode logOutput;
	    };

	    ///Calls method on JsonRpc interface
	    /**
	     *
	     * @param httpRequest pointer to IHttpRequest. This pointer is then appear on RpcRequest
	     * 			as httpRequest. You can set this argument to NULL but this can
	     * 			cause crash when called method expect the valid pointer. Aslo
	     * 			note that method can request various interfaces from this argument. If you
	     * 			are supply proxy object, don't forget to proxy interface requests to
	     * 			the original object.
	     * @param methodName Name of method to call
	     * @param args arguments as defined in JSONRPC protocol. Can't be set to NULL
	     * @param context optional context. This argument can be NULL, when no context is defined
	     * @param id identification of request. Can't be set to NULL
	     * @return Function returns struct that contains result and various items connected to result.
	     *      see CallResult struct
	     *
	     * @exception RpcCallError error in arguments.
	     *
	     * @note function will not throw user exception thrown outside of method unless RpcCallError
	     *  is thrown. Also unknown exceptions (such a ... or exceptions not inherited from
	     *  std::exception are thrown outside of this call
	     */
	    virtual CallResult callMethod(IHttpRequest *httpRequest, ConstStrA methodName, JSON::INode *args, JSON::INode *context, JSON::INode *id) = 0;


	    ///Tests whether object can accept request from specified origin
	    /** This is intended to share configuration of CORS from JsonRPC object to other objects calling the methods
	     * directly throigh callMethod()
	     * @param origin origin of the request
	     * @retval true origin is allowed
	     * @retval false origin is not allowed
	     * @retval null CORS options are disabled. Caller should check whether origin's domain match to server's domain.
	     *
	     * @note if CORS is not allowed for the JSONR
	     */
	    virtual Optional<bool> isAllowedOrigin(ConstStrA origin) = 0;

	    virtual ~IJsonRpc() {}

	};


}


#endif /* JSONRPC_IJSONRPC_H_ */
