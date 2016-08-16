/*
 * errors.h
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONRPCSERVER_qwo10986_ERRORS_H_
#define JSONRPC_JSONRPCSERVER_qwo10986_ERRORS_H_
#include <lightspeed/base/exceptions/exception.h>
#include <lightspeed/utils/json.h>

#include "lightspeed/base/containers/string.h"

namespace jsonrpc {

using namespace LightSpeed;

	class RpcException: public virtual LightSpeed::Exception {
	public:
		RpcException():LightSpeed::Exception(THISLOCATION) {}

		virtual JSON::ConstValue getJSON(const JSON::Builder &json) const = 0;
	};


	class LookupException: public RpcException {
	public:
		LIGHTSPEED_EXCEPTIONFINAL;
		LookupException(const ProgramLocation &loc, StringA prototype):LightSpeed::Exception(loc)
			,prototype(prototype) {}
		~LookupException() throw() {}

	protected:

		StringA prototype;

		void message(ExceptionMsg &msg) const;
		virtual JSON::ConstValue getJSON(const JSON::Builder &json) const;
	};

	class ParseException: public RpcException {
	public:
		LIGHTSPEED_EXCEPTIONFINAL;
		ParseException(const ProgramLocation &loc, StringA description):LightSpeed::Exception(loc)
			,description(description) {}
		~ParseException() throw() {}

	protected:

		StringA description;

		void message(ExceptionMsg &msg) const;
		virtual JSON::ConstValue getJSON(const JSON::Builder &json) const;
	};

	class MethodException: public RpcException {
	public:
		LIGHTSPEED_EXCEPTIONFINAL;
		MethodException(const ProgramLocation &loc, natural statusCode, StringA statusMessage)
			:Exception(loc),statusCode(statusCode),statusMessage(statusMessage) {}
		~MethodException() throw() {}
	protected:

		natural statusCode;
		StringA statusMessage;

		void message(ExceptionMsg &msg) const;
		virtual JSON::ConstValue getJSON(const JSON::Builder &json) const;
	};

	class UncauchException: public MethodException {
	public:
		LIGHTSPEED_EXCEPTIONFINAL;
		UncauchException(const ProgramLocation &loc, const Exception &e);
		virtual JSON::ConstValue getJSON(const JSON::Builder &json) const;

	protected:

		TypeInfo type;

	};




}



#endif /* JSONRPC_JSONRPCSERVER_qwo10986_ERRORS_H_ */
