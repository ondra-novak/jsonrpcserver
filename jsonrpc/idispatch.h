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
	virtual void operator()(const Request &, Promise<Response> ) const = 0;
	virtual ~IMethod() {}


};

///method handler with bound function.
/** It calls specified function. The function must accept const Request &, and
 * results either Response or Promise<Response> or simple ConstValue
 */
template<typename Fn>
class BoundFunction: public IMethod {
public:
	BoundFunction(const Fn &fn):fn(fn) {}
	void operator()(const Request &req, Promise<Response> response) const {
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
	void operator()(const Request &req, Promise<Response> response) const {
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
	///Registers stat handler, handler is not checked for aguments, the arguments of server.stat is passed directly
	/**
	 * @param name name of stat handler (no defintion arguments allowed)
	 * @param fn function handles the statistics
	 * @param untilVer defined when handler has been removed from the api. It is skipped, but
	 * other handlers with same name can be used
	 */
    virtual void regStatsHandler(ConstStrA name, IMethod *fn, natural untilVer=naturalNull) = 0;
    ///Unregister the stat handler.
    virtual void unregStats(ConstStrA name, natural ver=naturalNull) = 0;

    virtual PException setExceptionHandler(const std::exception &e) = 0;

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
