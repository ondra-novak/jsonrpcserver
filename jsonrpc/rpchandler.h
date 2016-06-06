/*
 * rpchandler.h
 *
 *  Created on: Sep 22, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSRV_RPCHANDLER_H_
#define JSONRPCSRV_RPCHANDLER_H_
#include "lightspeed/utils/json.h"
#include "lightspeed/base/memory/pointer.h"
#include <lightspeed/base/exceptions/exception.h>
#include "lightspeed/base/actions/message.h"
#include "../httpserver/httprequest.h"

#pragma once

namespace jsonsrv {

using namespace BredyHttpSrv;
using namespace LightSpeed;


class IJsonRpc;

struct RpcRequest {

	///Function name, which has been called
	ConstStrA functionName;
	///Format of arguments
	ConstStrA argFormat;
	///Message ID, when ID is string
	ConstStrA id;
	///Message ID node. If request is notify, this field is null
	JSON::Value idnode;
	///pointer to arguments. Always defined, even if arguments are empty
	/**
	 * @note Arguments should be stored in Array as defined in specification. Old implementation
	 * of Pry4 server allows pass argumemts as class. Handler should expect both forms
	 */
	JSON::Value args;
	///pointer to factory recomended to allocate new JSON nodes
	JSON::PFactory jsonFactory;
	///pointer to HTTP request, which contains this call
	/** This pointer can be sometimes nil, especially, when methos has not be invoked by HTTP request.
	 */
	Pointer<IHttpRequest> httpRequest;
	///Pointer to server stub providing this call
	Pointer<IJsonRpc> serverStub;
	///method context (JSONRPC extension)
	/** If context is not included to the request, pointer is null */
	JSON::Value context;
	///Context sent with reply
	/** This pointer is null by default. When method wants to send context as reply,
	 * it must create class node here using jsonFactory
	 */
	JSON::Value contextOut;
	///Data reported as output to logfile
	/** to report whole output, set this member same as return value.
	 * Default value NULL will disable output reporting
	 * Otherwise, any JSON value set to this node will be reported.
	 */
	JSON::Container logOutput;
	///User defined localdata carried through whole request including multicall reqiest
	JSON::Value localData;


	void setContext(const char *name, const JSON::Value &value) {
		if (contextOut == nil) contextOut = jsonFactory->newClass();
		contextOut.set(name,value);
	}
	void setLogOutput(const char *name, const JSON::ConstValue &value) {
		if (logOutput == nil) logOutput = jsonFactory->newClass();
		logOutput.set(name,value);
	}

	ConstStrA argStrA(natural n) const {
		return (*args)[n].getStringUtf8();
	}
	ConstStrW argStrW(natural n) const {
		return (*args)[n].getString();
	}

	integer argInt(natural n) const {
		return (*args)[n].getInt();
	}
	natural argUInt(natural n) const {
		return (*args)[n].getUInt();
	}
	double argFloat(natural n) const {
		return (*args)[n].getFloat();
	}
	bool argBool(natural n) const {
		return (*args)[n].getBool();
	}
	bool argNull(natural n) const {
		return (*args)[n].isNull();
	}
	JSON::PNode argObject(natural n) const {
		return args->getEntry(n);
	}

	typedef JSON::Iterator ArgIter;

	ArgIter getNamedArgs(natural n) const {
		return args->getEntry(n)->getFwIter();
	}


	JSON::Builder::Object object() const {return JSON::Builder(jsonFactory.get()).object();}
	JSON::Builder::Array array() const {return JSON::Builder(jsonFactory.get()).array();}
	JSON::PNode ok() const {return jsonFactory->newValue(true);}

};


typedef Message<JSON::PNode,  RpcRequest *> RpcCall;
typedef RpcCall::Ifc IRpcCall;


class RpcError: public Exception {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcError(const ProgramLocation &loc, JSON::Value errorNode);
	RpcError(const ProgramLocation &loc, JSON::IFactory *factory, natural status, String statusMessage);
	RpcError(const ProgramLocation &loc,const  RpcRequest *r, natural status, String statusMessage);
	~RpcError() throw() {}
	const JSON::Value &getError() const;

	void message(ExceptionMsg &msg) const;

protected:
	JSON::Value errorNode;
};

class RpcCallError: public Exception {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcCallError(const ProgramLocation &loc, natural status, StringA statusMessage)
		:Exception(loc),status(status),statusMessage(statusMessage) {}

	natural getStatus() const {return status;}
	const StringA &getStatusMessage() const {return statusMessage;}
	~RpcCallError() throw() {}

protected:
	natural status;
	StringA statusMessage;

	void message(ExceptionMsg &msg) const;

};


class RpcMandatoryField: public RpcError {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcMandatoryField(const ProgramLocation &loc, JSON::IFactory *factory, ConstStrA field)
		:RpcError(loc,factory,400,String(ConstStrA("Mandatory field: ")+field)),field(field) {}
	ConstStrA getField() const {return field;}
	~RpcMandatoryField() throw() {}
protected:
	StringA field;


};



}


#endif /* JSONRPCSRV_RPCHANDLER_H_ */

