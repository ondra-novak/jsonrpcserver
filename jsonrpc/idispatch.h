#pragma once
#include <lightspeed/base/interface.h>
#include "lightspeed/base/containers/constStr.h"
#include <lightspeed/utils/json/json.h>

#include "../httpserver/httprequest.h"
#include "lightspeed/base/actions/promise.h"

#include "ijsonrpc.h"
#include "lightspeed/base/exceptions/stdexception.h"

#include "lightspeed/base/memory/weakref.h"
namespace jsonrpc {


using namespace LightSpeed;

class IDispatcher;
class ILog;
class IRpcNotify;
class IClient;

class IRequestContext: public BredyHttpSrv::IHttpHandlerContext {};

///Object passed to the every call of the every method
/** The request can be copied as a value. It contains shared or weak references
 * to other objects. Shared references are valid indefently, because they are
 * ref-counter. Weak references can disappear, regadless on how many copies of the
 * request has been created. Always check weak reference for its value. Never lock
 * weak refernce for long time.
 */
struct Request {

	///Name of the method
	/** the name is carried as ConstValue which always contains a string. */
	JSON::ConstValue methodName;
	///parameters
	/** the parameters are carried as ConstValue which always contains an array */
	JSON::ConstValue params;
	///Client side identification. If this is null-value (not null-pointer), the call is probably notification only
	/** the ID can be anything, it is object created by caller and must be returned to the caller
	 */
	JSON::ConstValue id;
	///context passed with the call
	JSON::ConstValue context;
	///json builder to build result
	/** For convience, this builder can be used to easly create JSON response */
	JSON::Builder json;
	///true if call is notification. However the call still must resolve the promise
	/** In most cases, isNotification will be true, while id is set to null.
	 * Even if notifications has no return value, the method itself should resolve
	 * promise, or return some value. It can return instance od Void as return value.
	 * Failing this requirement can cause a memory leak.
	 */
	bool isNotification;

	///pointer to HTTP interface.
	/** This is only class which represents context of the request. It is tied to
	 * current connection which created the request and persists until the connection
	 * is closed or server stopped. This is very important for methods performing
	 * asynchronous operations. If connection disappear during the call, reference
	 * to the httpRequest becomes invalid and any access to it causes access violation.
	 * The asynchronous method can continue even if the request is lost, however, it cannot
	 * access the original request and any result returned by the method is discarded.
	 *
	 * You can easy install a handler, which is called before the connection is closed.
	 * See function setRequestContext
	 *
	 */
	WeakRef<BredyHttpSrv::IHttpContextControl> httpRequest;
	///pointer to dispatcher
	/** The dispatcher instances should unlikely disappear, however, if this can happen
	 * for example in case that some task running beyond lifetime of whole server,
	 * you should check, whether this value is not NULL before you access it.
	 *
	 * The dispatcher itself has not much useful methods, however, most server implementation
	 * offers access to other interfaces through the getIfc() function. You
	 * can access for example IMethodRegister. The dispatcher can be used to call methods
	 * from inside of other method.
	 */
	WeakRef<IDispatcher> dispatcher;


	///Contains reference to the IRpcNotify - it is responsible to deliver rpc notifications
	/** If this reference is null, then no notify service is available. Otherwise you can
	 * use this service to send notifications to the client. If you plan to send notifications
	 * later outside to current method, you should store the reference as WeakRef. When
	 * the client disconnects, this reference is set to null
	 */
	WeakRef<IRpcNotify> notifySvc;


	///Contains reference to the IClient object if it is implemented
	/** Some connections (websockets) supports reversed RPC, where the client processes methods
	 * requested by the server. If the client interface is available, this reference is set
	 * to non-null value
	 */
	WeakRef<IClient> client;


	///Sets context of the request
	/**
	 * @param ctx pointer to any object which implements IRequestContext. The object
	 * must be allocated, because the server receives its ownership and it is destroyed
	 * when it is no longer needed. Also note, that context is destroyed when connection
	 * is closed, this is very important for methods performing asynchronous task.
 	 * Destroying the context invokes context's destructor and the method can perform
 	 * necesery actions to prevent any future problems.  During destruction, the
 	 * original request is still available.
 	 *
 	 * @note function destroyes any context already there. Also note that context
 	 * is destroyed after the result is delivered to the caller. If result is delivered
 	 * through the promise, accessing anything in the request after the promise is resolved
 	 * causes undefined behaviour
	 *
	 */
	void setRequestContext(IRequestContext *ctx) {
		httpRequest.lock()->setRequestContext(ctx);
	}

	IRequestContext *getRequestContext() {
		return dynamic_cast<IRequestContext *>(httpRequest.lock()->getRequestContext());
	}

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


typedef Future<Response> FResponse;

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


    ///Dispatches exception
    /** Appends exception dispatching to result future.
     *
     * @param req request
     * @param result result from callMethod
     * @return new result (contains or will contain dispatched exception)
     */
    virtual Future<Response> dispatchException(const Request &req, Future<Response> result) throw() = 0;


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
    		const JSON::Builder &json,
			const WeakRef<BredyHttpSrv::IHttpContextControl> &requestrequest) throw()= 0;

};



}

