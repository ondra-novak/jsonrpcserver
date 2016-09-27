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
	Response(JSON::ConstValue result)
		:result(result) {}
	///construct response with context
	Response(JSON::ConstValue result,JSON::ConstValue context)
		:result(result),context(context) {}
	///construct response with context and log data
	Response(JSON::ConstValue result,JSON::ConstValue context,JSON::ConstValue logOutput)
		:result(result),context(context),logOutput(logOutput) {}

	Response getMt() const {
		return Response(result.getMT(), context.getMT(), logOutput.getMT());
	}
};
typedef jsonsrv::IJsonRpcLogObject ILog;

///Dispatches JSONRPC request to the handler, registers handlers, etc.
class IDispatcher: public virtual IInterface {
public:
	///Calls RPC method
	/**
	 * @param req request (contains method-name, argument etc)
	 * @return future variable with response. It can be resolved or not(yet)
	 *
	 */
    virtual Future<Response> callMethod(const Request &req) throw() = 0;

    ///Dispatches exception
    /**
     * Reason to dispatch exception is if you need to carry exception through the RPC protocol. Without dispatching
     * every exception is considered as internal error (with 500 as status code).
     * There is only one exception - MethodException - with can have different status code. Exception dispatching
     * helps you to keep internal structure of the exceptions untouch and allows you tu convert exceptions
     * into normalized JSONRPC error messages
     *
     * @param req requests which generated an exception
     * @param exception pointer to the exception
     * @return function must return resolved or unresolved future, which carries the new result
     * or an exception. If future is resolved by exception, it should be MethodException to emit
     * correct error status. Other exceptions are emitted as internal errors.
     * The function have to return Future(null) to express, that it cannot handle the exception. In this
     * case, next handler is called.
     *
     * The function will convert the exception into an UncaughtException
     * when there are no more handlers to resolve the exception
     *
     */
    virtual Future<Response> dispatchException(const Request &req, const PException &exception) throw() = 0;


    ///Calls RPC method directly from JSON message. Result is also JSON message which is ready to transfer back to the client
    /**
     * Function retrieves required arguments and executes method through the callMethod. Result
     * is passed through the Promise
     * @param jsonrpcmsg JSON message contains everything need to process JSONRPC call
     * @param json reference to JSON builder (must be MT safe)
     * @param request http request if available (NULL, if not)
     * @return Future variable resolved or unresolved yet
     */
    virtual Future<JSON::ConstValue> dispatchMessage(const JSON::ConstValue jsonrpcmsg,
    		const JSON::Builder &json, BredyHttpSrv::IHttpRequestInfo *request) throw()= 0;

};



}

