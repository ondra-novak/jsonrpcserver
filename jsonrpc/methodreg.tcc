/*
 * idispatch.tcc
 *
 *  Created on: 15. 8. 2016
 *      Author: ondra
 */

#pragma once

#include "methodreg.h"

namespace jsonrpc {

template<typename Fn>
void IMethodRegister::regMethod(ConstStrA method,const Fn &fn) {

	///method handler with bound function.
	/** It calls specified function. The function must accept const Request &, and
	 * results either Response or Promise<Response> or simple ConstValue
	 */
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


	regMethodHandler(method,new BoundFunction(fn));
}

template<typename ObjPtr, typename Obj, typename Ret>
struct IMethodRegister::MethodCallMemberObject {



	ObjPtr objPtr;
	Ret (Obj::*fn)(const Request &);

	Ret operator()(const Request &req) const {
		return (objPtr->*fn)(req);
	}
	MethodCallMemberObject(	ObjPtr objPtr,	Ret (Obj::*fn)(const Request &))
		:objPtr(objPtr),fn(fn) {}
};

template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
struct IMethodRegister::MethodCallMemberObjectArg {

	ObjPtr objPtr;
	Ret (Obj::*fn)(const Request &, const Arg &arg);
	Arg arg;

	Ret operator()(const Request &req) const {
		return (objPtr->*fn)(req,arg);
	}
	MethodCallMemberObjectArg(	ObjPtr objPtr,	Ret (Obj::*fn)(const Request &), const Arg &arg)
		:objPtr(objPtr),fn(fn),arg(arg) {}
};


template<typename ObjPtr, typename Obj, typename Ret>
void IMethodRegister::regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &)) {
	regMethod(method,MethodCallMemberObject<ObjPtr,Obj,Ret>(objPtr,fn));
}

template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
void IMethodRegister::regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &, const Arg &arg), const Arg &arg) {
	regMethod(method,MethodCallMemberObjectArg<ObjPtr,Obj,Ret,Arg>(objPtr,fn,arg));
}

template<typename Fn>
void IMethodRegister::regException(ConstStrA name,const Fn &fn) {


	///method handler with bound function.
	/** It calls specified function. The function must accept const Request &, and
	 * results either Response or Promise<Response> or simple ConstValue
	 */
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

	regExceptionHandler(name,new BoundExceptionHandler(fn));


}

template<typename ObjPtr, typename Obj, typename Ret>
struct IMethodRegister::ExceptionCallMemberObject {

	typedef Ret (Obj::*Fn)(const Request &req, const PException &excp, Promise<Response> response);
	ObjPtr objPtr;
	Fn fn;

	Ret operator()(const Request &req, const PException &excp, Promise<Response> response) const {
		return (objPtr->*fn)(req,excp,response);
	}
	ExceptionCallMemberObject(const	ObjPtr &objPtr,	const Fn &fn)
		:objPtr(objPtr),fn(fn) {}

};
template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
struct IMethodRegister::ExceptionCallMemberObjectArg{

	typedef Ret (Obj::*Fn)(const Request &req, const PException &excp, Promise<Response> response, const Arg &arg);
	ObjPtr objPtr;
	Fn fn;
	Arg arg;

	Ret operator()(const Request &req, const PException &excp, Promise<Response> response) const {
		return (objPtr->*fn)(req,excp,response,arg);
	}
	ExceptionCallMemberObjectArg(const	ObjPtr &objPtr,	const Fn &fn, const Arg &arg)
		:objPtr(objPtr),fn(fn),arg(arg) {}

};


template<typename ObjPtr, typename Obj, typename Ret>
void IMethodRegister::regException(ConstStrA name, const ObjPtr &objPtr,
		bool (Obj::*fn)(const Request &, const PException &, Promise<Ret> )) {
	regException(name, ExceptionCallMemberObject<ObjPtr,Obj,Ret>(objPtr,fn));

}

template<typename ObjPtr, typename Obj, typename Ret, typename Arg>
void IMethodRegister::regException(ConstStrA name, const ObjPtr &objPtr,
		bool (Obj::*fn)(const Request &, const PException &, Promise<Ret>, const Arg &arg), const Arg &arg) {
	regException(name, ExceptionCallMemberObjectArg<ObjPtr,Obj,Ret,Arg>(objPtr,fn,arg));

}




}