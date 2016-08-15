/*
 * idispatch.tcc
 *
 *  Created on: 15. 8. 2016
 *      Author: ondra
 */

#pragma once

#include "idispatch.h"

namespace jsonrpc {

template<typename Fn>
void IMethodRegister::regMethod(ConstStrA method,const Fn &fn,natural ver) {

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


	regMethodHandler(method,new BoundFunction(fn),ver);
}
template<typename ObjPtr, typename Obj, typename Ret>
void IMethodRegister::regMethod(ConstStrA method, const ObjPtr &objPtr, Ret (Obj::*fn)(const Request &),natural ver) {

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


	regMethodHandler(method,new BoundMemberFunction(objPtr,fn),ver);
}

template<typename Fn>
void IMethodRegister::regException(ConstStrA name,const Fn &fn,natural ver) {


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

	regExceptionHandler(name,new BoundExceptionHandler(fn),ver);


}
template<typename ObjPtr, typename Obj, typename Ret>
void IMethodRegister::regException(ConstStrA name, const ObjPtr &objPtr,
		bool (Obj::*fn)(const Request &, const PException &, Promise<Ret> ),
		natural ver) {
	///Method handle with bound member function

	class BoundMemberExceptionHandler: public IExceptionHandler {
	public:
		typedef Ret (Obj::*FnRef)(const Request &, const PException &, Promise<Ret> );

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


	regExceptionHandler(name,new BoundMemberExceptionHandler(objPtr,fn),ver);

}




}
