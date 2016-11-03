/*
 * iclient.h
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_JSONRPC_ICLIENT_H_
#define LIBS_JSONRPCSERVER_JSONRPC_ICLIENT_H_
#include <lightspeed/utils/json/json.h>

#include "../httpclient/httpClient.h"
#include "lightspeed/base/actions/promise.h"

#include "json.h"
namespace jsonrpc {

using namespace LightSpeed;

class IClient {
public:

	class Result: public JValue {
	public:
		Result(JValue result, JValue context): JValue(result),context(context) {}
		JValue context;
	};
	virtual ~IClient() {}

	///Performs RPC call asynchronously
	/**
	 *
	 * @param jsonFactory reference to JSON factory which will be used to parse response and create JSON result. Note that
	 *  factory object must be MT save, or you should avoid to access it until the result is received
	 * @param method name of the method
	 * @param params parameters, must be either array or object
	 * @param context object which contains
	 * @return function returns future which can be resolved later
	 *
	 *
	 */
	virtual Future<Result> callAsync(StrView method, JValue params, JValue context = JValue()) = 0;


	///Performs RPC call synchronously
	/**
	 *
	 * @param method method to call
	 * @param params arguments of the method
	 * @param context context (optional)
	 * @return result can be convertible to JValue however it contains a context if returned
	 */
	virtual Result call(StrView method, JValue params, JValue context = 0) {
		return callAsync(method, params, context).getValue();
	}

};





}




#endif /* ICLIENT_H_ */
