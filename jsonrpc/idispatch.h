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

///method handler with bound function.
/** It calls specified function. The function must accept const Request &, and
 * results either Response or Promise<Response> or simple ConstValue
 */
template<typename Fn>
class BoundFunction: public IMethod {
public:
	BoundFunction(const Fn &fn):fn(fn) {}
	void operator()(const Request &req, Promise<Response> response) const throw() {
		try {
			response.resolve(fn(req));
		} catch (Exception &e) {
			response.reject(e);
		} catch (std::exception  &e) {
			response.reject(StdException(THISLOCATION,e));
		} catch (...) {
			response.reject(UnknownException(THISLOCATION));
		}
	}
protected:
	Fn fn;
};

///Method handle with bound member function

template<typename ObjPtr, typename Obj, typename Ret>
class BoundMemberFunction: public IMethod {
public:
	typedef Ret (Obj::*FnRef)(const Request &req);

	BoundMemberFunction(const ObjPtr objRef, FnRef fn):objRef(objRef),fn(fn) {}
	void operator()(const Request &req, Promise<Response> response) const throw() {
		try {
			response.resolve((objRef->*fn)(req));
		} catch (Exception &e) {
			response.reject(e);
		} catch (std::exception  &e) {
			response.reject(StdException(THISLOCATION,e));
		} catch (...) {
			response.reject(UnknownException(THISLOCATION));
		}
	}
protected:
	ObjPtr objRef;
	FnRef fn;
};

///method handler with bound function.
/** It calls specified function. The function must accept const Request &, and
 * results either Response or Promise<Response> or simple ConstValue
 */
template<typename Fn>
class BoundExceptionHandler: public IExceptionHandler {
public:
	BoundExceptionHandler(const Fn &fn):fn(fn) {}
	bool operator()(const Request &req, const PException &excp, Promise<Response> response) const throw() {
		try {
			return fn(req,excp,response);
		} catch (Exception &e) {
			response.reject(e);
		} catch (std::exception  &e) {
			response.reject(StdException(THISLOCATION,e));
		} catch (...) {
			response.reject(UnknownException(THISLOCATION));
		}
	}
protected:
	Fn fn;
};

///Method handle with bound member function

template<typename ObjPtr, typename Obj, typename Ret>
class BoundMemberExceptionHandler: public IExceptionHandler {
public:
	typedef Ret (Obj::*FnRef)(const Request &req, const PException &excp, Promise<Response> response);

	BoundMemberExceptionHandler(const ObjPtr objRef, FnRef fn):objRef(objRef),fn(fn) {}
	bool operator()(const Request &req, const PException &excp, Promise<Response> response) const throw() {
		try {
			return (objRef->*fn)(req,excp,response);
		} catch (Exception &e) {
			response.reject(e);
		} catch (std::exception  &e) {
			response.reject(StdException(THISLOCATION,e));
		} catch (...) {
			response.reject(UnknownException(THISLOCATION));
		}
	}
protected:
	ObjPtr objRef;
	FnRef fn;
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
	 */
    virtual void callMethod(const Request &req, Promise<Response> result) throw() = 0;


    ///Calls RPC method directly from JSON
    /**
     * Function retrieves required arguments and executes method through the callMethod. Result
     * is passed through the Promise
     * @param jsonrpcmsg JSON message contains everything need to process JSONRPC call
     * @param version version of the api used (carried outside of message)
     * @param json reference to JSON builder (must be MT safe)
     * @param request http request if available (NULL, if not)
     * @param result result is stored here
     * @return message-id - because message ID is not carried through the result, you need
     *  pass the ID to the result handler.
     */
    virtual JSON::ConstValue dispatchMessage(const JSON::ConstValue jsonrpcmsg, natural version, const JSON::Builder &json, BredyHttpSrv::IHttpRequestInfo *request, Promise<Response> result) throw()= 0;


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


	virtual void regExceptionHandler(IMethod *fn, natural untilVer) = 0;

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
    void regMethod(ConstStrA method,const Fn &fn,natural ver=1) {
    	regMethodHandler(method,new BoundFunction<Fn>(fn),ver);
    }
    template<typename ObjPtr, typename Obj, typename Ret>
    void regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &),natural ver=1) {
    	regMethodHandler(method,new BoundMemberFunction<ObjPtr,Obj,Ret>(objPtr,fn),ver);
    }

    template<typename Fn>
    void regStats(ConstStrA method,const Fn &fn,natural ver=1) {
    	regStatsHandler(method,new BoundFunction<Fn>(fn),ver);
    }
    template<typename ObjPtr, typename Obj, typename Ret>
    void regStats(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &),natural ver=1) {
    	regStatsHandler(method,new BoundMemberFunction<ObjPtr,Obj,Ret>(objPtr,fn),ver);
    }


};


}
