#pragma once
#include "lightspeed/base/containers/constStr.h"
#include <lightspeed/utils/json/json.h>

#include "../httpserver/httprequest.h"
#include "lightspeed/base/actions/promise.h"

#include "ijsonrpc.h"
namespace jsonrpc {


using namespace LightSpeed;

class IDispatch;

struct Request {

	ConstStrA methodName;
	JSON::ConstValue params;
	JSON::ConstValue id;
	JSON::ConstValue context;
	JSON::Builder json;

	Pointer<BredyHttpSrv::IHttpRequestInfo> httpRequest;
	Pointer<IDispatch> dispatcher;
};

struct Response {
	const JSON::ConstValue result;
	const JSON::ConstValue context;
	const JSON::ConstValue logOutput;

	Response(JSON::ConstValue result);
	Response(JSON::ConstValue result,JSON::ConstValue context);
	Response(JSON::ConstValue result,JSON::ConstValue context,JSON::ConstValue logOutput);
};

class IMethod {
public:
	virtual void operator()(const Request &, Promise<Response> ) const = 0;
	virtual ~IMethod() {}


};

typedef jsonsrv::IJsonRpcLogObject ILog;

///Dispatches JSONRPC request to the handler, registers handlers, etc.
class IDispatch {
public:

	virtual void regMethod(ConstStrA method, const IMethod *fn, natural ver=1) = 0;
	virtual void unregMethod(ConstStrA method, natural ver=1) = 0;
    virtual void regStats(ConstStrA name, const IMethod *fn, natural ver=1) = 0;
    virtual void unregStats(ConstStrA name, natural ver=1) = 0;
    virtual void setLogObject(ILog *logObject) = 0;
    virtual void setRequestMaxSize(natural bytes) = 0;
    virtual void callMethod(const Request &req, Promise<Response> response) = 0;

    template<typename Fn>
	class BoundFunction: public IMethod {
	public:
    	BoundFunction(const Fn &fn):fn(fn) {}
		void operator()(const Request &req, Promise<Response> response) const {
			response.resolve(fn(req));
		}
	protected:
		Fn fn;
	};

    template<typename ObjPtr, typename Obj, typename Ret>
	class BoundMemberFunction: public IMethod {
	public:
    	typedef Ret (Obj::*FnRef)(const Request &req);

    	BoundMemberFunction(const ObjPtr objRef, FnRef fn):objRef(objRef),fn(fn) {}
		void operator()(const Request &req, Promise<Response> response) const {
			response.resolve((objRef->*fn)(req));
		}
	protected:
		ObjPtr objRef;
		FnRef fn;
	};


    template<typename Fn>
    void regMethod(ConstStrA method,const Fn &fn,natural ver=1) {
    	regMethod(method,static_cast<IMethod *>(new BoundFunction<Fn>(fn)),ver);
    }
    template<typename ObjPtr, typename Obj, typename Ret>
    void regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &),natural ver=1) {
    	regMethod(method,static_cast<IMethod *>(new BoundMemberFunction<ObjPtr,Obj,Ret>(objPtr,fn)),ver);
    }

    template<typename Fn>
    void regStats(ConstStrA method,const Fn &fn,natural ver=1) {
    	regStats(method,static_cast<IMethod *>(new BoundFunction<Fn>(fn)),ver);
    }
    template<typename ObjPtr, typename Obj, typename Ret>
    void regStats(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &),natural ver=1) {
    	regStats(method,static_cast<IMethod *>(new BoundMemberFunction<ObjPtr,Obj,Ret>(objPtr,fn)),ver);
    }





};


}
