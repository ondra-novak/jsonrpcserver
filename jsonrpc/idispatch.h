#pragma once
#include <lightspeed/base/interface.h>
#include "lightspeed/base/containers/constStr.h"
#include <lightspeed/utils/json/json.h>

#include "../httpserver/httprequest.h"
#include "lightspeed/base/actions/promise.h"

#include "ijsonrpc.h"
#include "lightspeed/base/exceptions/stdexception.h"
namespace jsonrpc {


using namespace LightSpeed;

class IDispatcher;

///Object passed to the every call of the every method
struct Request {

	///Name of the method
	ConstStrA methodName;
	///parameters
	JSON::ConstValue params;
	///Client side identification. If this is null-value (not null-pointer), the call is probably notification only
	JSON::ConstValue id;
	///context passed with the call
	JSON::ConstValue context;
	///json builder to build result
	JSON::Builder json;
	///version of the API
	natural version;
	///true if call is notification. However the call still must resolve the promise
	bool isNotification;

	///pointer to HTTP interface. Can be NULL
	Pointer<BredyHttpSrv::IHttpRequestInfo> httpRequest;
	///pointer to dispatcher
	Pointer<IDispatcher> dispatcher;
};

///Contains result of the call
/** Note Errors are passed as rejection of the promise or as an exception */
struct Response {
	///contains result
	const JSON::ConstValue result;
	///contains update of the context, can be null if not applied
	const JSON::ConstValue context;
	///contains data to be logged into rpclog, can be null if not applied
	/** Method can log different result than returned to the caller. In most of
	 * cases, methods will log only errors. However it is possible to log result
	 *  or something else. It must be json.
	 */
	const JSON::ConstValue logOutput;

	///construct only response
	Response(JSON::ConstValue result);
	///construct response with context
	Response(JSON::ConstValue result,JSON::ConstValue context);
	///construct response with context and log data
	Response(JSON::ConstValue result,JSON::ConstValue context,JSON::ConstValue logOutput);

	Response getMt() const {
		return Response(result.getMT(), context.getMT(), logOutput.getMT());
	}
};

///Abstract method handler
class IMethod: public RefCntObj {
public:

	///Calls the method
	/**
	 * @param request request from the client, also contains the parameters
	 * @param response promise which might be resolved with a result. However it is allowed
	 * to left promise unresolved and resolve it later. By leaving promise unresolved, the current
	 * thread is released for other requests waiting in the queue.
	 */
	virtual void operator()(const Request &, Promise<Response> ) const throw() = 0;
	virtual ~IMethod() {}


};

///Abstract exception handler
/** This exception handler is called, when method throws an exception which is unknown
 * to the dispatcher. The exception handler can transform exception to an RpcException, or
 * it can return a default response after the exception
 */
class IExceptionHandler: public RefCntObj {
public:
	///Handler is executed with reference to pointer to exception
	/**
	 * @param request pointer to the request
	 * @param exception pointer to exception
	 * @param response reference to promise to resolve
	 * @retval true exception has been resolved. However, the promise can remain unresolved. In such
	 * case, the dispatcher expect, that the exception will be resolved later.
	 * @retval false exception has not been resolved. The dispatcher will call other handler
	 *
	 * Function can left promise unresolved. In this case, next exception
	 */
	virtual bool operator()(const Request &, const PException &, Promise<Response> ) const throw() = 0;
	virtual ~IExceptionHandler() {}
};

typedef jsonsrv::IJsonRpcLogObject ILog;

///Dispatches JSONRPC request to the handler, registers handlers, etc.
class IDispatcher: public virtual IInterface {
public:
	///Calls RPC method
	/**
	 * @param req request (contains method-name, argument etc)
	 * @param result result of the call. Note that promise can be unresolved, because method
	 *  may be resolved asynchronously
	 *
	 *  @note exception is not dispatched. You need to dispatch exception by calling the function dispatchException()
	 *
	 */
    virtual void callMethod(const Request &req, Promise<Response> result) throw() = 0;

    ///Dispatches exception
    /**
     * Reason to dispatch exception is if you need to carry exception through the RPC protocol. Without dispatching
     * every exception is considered as internal error (with 500 as status code).
     * There is only exception - MethodException - with can have different status code. Exception dispatching
     * helps you to keep internal structure of the exceptions untouch and allows you tu convert exceptions
     * into normalized JSONRPC error messages
     *
     * @param req requests which generated an exception
     * @param exception pointer to the exception
     * @param result promise which must be resolved by this call (now or later)
     *
     * function will convert exception into UncaughtException if none handler resolves the exception
     *
     */
    virtual void dispatchException(const Request &req, const PException &exception, Promise<Response> result) throw() = 0;


    ///Calls RPC method directly from JSON message. Result is also JSON message which is ready to ship
    /**
     * Function retrieves required arguments and executes method through the callMethod. Result
     * is passed through the Promise
     * @param jsonrpcmsg JSON message contains everything need to process JSONRPC call
     * @param version version of the api used (carried outside of message)
     * @param json reference to JSON builder (must be MT safe)
     * @param request http request if available (NULL, if not)
     * @param result result is stored here
     */
    virtual void dispatchMessage(const JSON::ConstValue jsonrpcmsg, natural version,
    		const JSON::Builder &json, BredyHttpSrv::IHttpRequestInfo *request,
			Promise<JSON::ConstValue> result) throw()= 0;



};



class IMethodRegister: public virtual IInterface{
public:


	///Register method handler
	/**
	 * @param method method name (and optionally, format of arguments)
	 * @param fn pointer to object, which will handle this method call
	 * @param untilVer version when this method has been removed from the api. If there
	 *  is same method with higher version, it will overwrite old method from specified version
	 *  above.
	 *
	 * @note method can contain description of arguments. Currently, only old system is implemented
	 * with following modifications
	 * @code
	 *    s - string
	 *    n - number
	 *    b - bool
	 *    x - null
	 *    d - string or number
	 *    a - array
	 *    o - object (not class)
	 *    u - undefined
	 *    * - repeated last
	 *    ? - any
	 * @endcode
	 *
	 * there can be comma , to create alternatives "ss,ssn,ssnb,ssb"
	 *
	 */
	virtual void regMethodHandler(ConstStrA method, IMethod *fn, natural untilVer=naturalNull) = 0;
	///Unregister method (you must supply correct arguments as well)
	/**
	 * @param method method name (including arguments)
	 * @param ver version (must be equal to version used during registration)
	 */
	virtual void unregMethod(ConstStrA method, natural ver=naturalNull) = 0;


	virtual void regExceptionHandler(ConstStrA name, IExceptionHandler *fn, natural untilVer = naturalNull) = 0;
	virtual void unregExceptionHandler(ConstStrA name, natural untilVer = naturalNull) = 0;


    class IMethodEnum {
    public:
    	virtual void operator()(ConstStrA prototype, natural version) const = 0;
    };

    virtual void enumMethods(const IMethodEnum &enm) const  = 0;


    ///change log object
    /**
     * @param logObject use null, to disable logging
     */
    virtual void setLogObject(ILog *logObject) = 0;

    template<typename Fn>
    void regMethod(ConstStrA method,const Fn &fn,natural ver=1);
    template<typename ObjPtr, typename Obj, typename Ret>
    void regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &),natural ver=1);

    template<typename Fn>
    void regException(ConstStrA method,const Fn &fn,natural ver=1);
    template<typename ObjPtr, typename Obj, typename Ret>
    void regException(ConstStrA method, const ObjPtr &objPtr, bool (Obj::*fn)(const Request &, const PException &, Promise<Ret> ),natural ver=1);



};


}
