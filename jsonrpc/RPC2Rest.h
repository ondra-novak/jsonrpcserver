/*
 * RPC2Rest.h
 *
 *  Created on: 14. 1. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONSRV_RPC2REST_H_
#define JSONRPCSERVER_JSONSRV_RPC2REST_H_
#include "../httpserver/httprequest.h"
#include "lightspeed/base/containers/stringKey.h"

#include "lightspeed/base/containers/map.h"

#include "lightspeed/base/exceptions/genexcept.h"
namespace jsonsrv {
class IJsonRpc;
class IJsonRpcLogObject;

using namespace LightSpeed;

class RPC2Rest: public BredyHttpSrv::IHttpHandler {
public:
	RPC2Rest(IJsonRpc &jsonrpc, IJsonRpcLogObject *logobject);

	void addMethod(ConstStrA methodName, ConstStrA argMapping);
	void addMethod(ConstStrA vpath, ConstStrA httpMethod, ConstStrA methodName, ConstStrA argMapping);

	virtual natural onRequest(BredyHttpSrv::IHttpRequest &request, ConstStrA vpath);
	virtual natural onData(BredyHttpSrv::IHttpRequest &) {return 0;}

protected:
	IJsonRpc &jsonrpc;

	struct Key {
		StringKey<StringA> vpath;
		StringKey<StringA> method;

		Key(const StringKey<StringA> &vpath, const StringKey<StringA> &method)
			:vpath(vpath),method(method) {}

	};

	struct CmpKeys {
		bool operator()(const Key &a, const Key &b) const;
	};

	struct Mapping {
		StringA methodName;
		StringA argMapping;

		Mapping(StringA methodName, StringA argMapping)
			:methodName(methodName),argMapping(argMapping) {}
	};

	typedef Map<Key, Mapping, CmpKeys> RestMap;
	RestMap restMap;
	IJsonRpcLogObject *logobject;


public:
	static const char *cantParseMember;
	typedef GenException1<cantParseMember, StringA> CannotParseMemberException;

};

} /* namespace jsonsrv */

#endif /* JSONRPCSERVER_JSONSRV_RPC2REST_H_ */
