/*
 * handler.h
 *
 *  Created on: 7. 8. 2016
 *      Author: ondra
 */

#ifndef HTTPSERVER_BIND_TCC_158qsxw54q6eqwekxjq45q5we4qweq435
#define HTTPSERVER_BIND_TCC_158qsxw54q6eqwekxjq45q5we4qweq435

#include "httprequest.h"

namespace BredyHttpSrv {

template<typename Fn>
IHttpHandler *IHttpHandler::bindFn(const Fn &fn) {

	class Handler: public IHttpHandler {
	public:
		Handler(const Fn &fn):fn(fn) {}

		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath) {
			return fn(request,vpath);
		}
		virtual natural onData(IHttpRequest &request) {
			return fn(request,ConstStrA());
		}
		virtual void release() throw() {
			delete this;
		}


	protected:
		Fn fn;
	};
	return new Handler(fn);

}

template<typename ObjPtr, typename Obj>
IHttpHandler *IHttpHandler::bindFn(ObjPtr obj, natural (Obj::*fn)(IHttpRequest &request, ConstStrA vpath)) {
	class Handler: public IHttpHandler {
	public:
		Handler(ObjPtr obj, natural (Obj::*fn)(IHttpRequest &request, ConstStrA vpath)):fn(fn),obj(obj) {}

		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath) {
			return (obj->*fn)(request,vpath);
		}
		virtual natural onData(IHttpRequest &request) {
			return (obj->*fn)(request,ConstStrA());
		}
		virtual void release() throw() {
			delete this;
		}


	protected:
		natural (Obj::*fn)(IHttpRequest &request, ConstStrA vpath);
		ObjPtr obj;
	};
	return new Handler(obj,fn);

}


template<typename Fn>
IHttpHandler *IHttpHandler::bindFnShared(const Fn &fn) {

	class Handler: public IHttpHandler {
	public:
		Handler(const Fn &fn):fn(fn) {}

		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath) {
			return fn(request,vpath);
		}
		virtual natural onData(IHttpRequest &request) {
			return fn(request,ConstStrA());
		}


	protected:
		Fn fn;
	};
	return new Handler(fn);

}

template<typename ObjPtr, typename Obj>
IHttpHandler *IHttpHandler::bindFnShared(ObjPtr obj, natural (Obj::*fn)(IHttpRequest &request, ConstStrA vpath)) {
	class Handler: public IHttpHandler {
	public:
		Handler(ObjPtr obj, natural (Obj::*fn)(IHttpRequest &request, ConstStrA vpath)):fn(fn),obj(obj) {}

		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath) {
			return (obj->*fn)(request,vpath);
		}
		virtual natural onData(IHttpRequest &request) {
			return (obj->*fn)(request,ConstStrA());
		}


	protected:
		natural (Obj::*fn)(IHttpRequest &request, ConstStrA vpath);
		ObjPtr obj;
	};
	return new Handler(obj,fn);

}

}



#endif /* HTTPSERVER_BIND_TCC_158qsxw54q6eqwekxjq45q5we4qweq435 */
