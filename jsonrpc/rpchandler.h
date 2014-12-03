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
	Pointer<JSON::INode> idnode;
	///pointer to arguments. Always defined, even if arguments are empty
	/**
	 * @note Arguments should be stored in Array as defined in specification. Old implementation
	 * of Pry4 server allows pass argumemts as class. Handler should expect both forms
	 */
	Pointer<JSON::INode> args;
	///pointer to factory recomended to allocate new JSON nodes
	Pointer<JSON::IFactory> jsonFactory;
	///pointer to HTTP request, which contains this call
	/** This pointer can be sometimes nil, especially, when methos has not be invoked by HTTP request.
	 */
	Pointer<IHttpRequest> httpRequest;
	///Pointer to server stub providing this call
	Pointer<IJsonRpc> serverStub;
	///method context (JSONRPC extension)
	/** If context is not included to the request, pointer is null */
	Pointer<JSON::INode> context;
	///Context sent with reply
	/** This pointer is null by default. When method wants to send context as reply,
	 * it must create class node here using jsonFactory
	 */
	JSON::PNode contextOut;
	///Data reported as output to logfile
	/** to report whole output, set this member same as return value.
	 * Default value NULL will disable output reporting
	 * Otherwise, any JSON value set to this node will be reported.
	 */
	JSON::PNode logOutput;
	///User defined localdata carried through whole request including multicall reqiest
	JSON::PNode localData;


	void setContext(const char *name, JSON::PNode value) {
		if (contextOut == nil) contextOut = jsonFactory->newClass();
		contextOut->add(name,value);
	}
	void setLogOutput(const char *name, JSON::PNode value) {
		if (logOutput == nil) logOutput = jsonFactory->newClass();
		logOutput->add(name,value);
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

	 ConstStrA argStrA(ArgIter &iter, ConstStrA name);
	ConstStrW argStrW(ArgIter &iter, ConstStrA name);
	integer argInt(ArgIter &iter, ConstStrA name);
	natural argUInt(ArgIter &iter, ConstStrA name);
	double argFloat(ArgIter &iter, ConstStrA name);
	bool argBool(ArgIter &iter, ConstStrA name);
	JSON::PNode argObject(ArgIter &iter, ConstStrA name);
	///Tests, whethe next argument is specified name and is NULL
	/** In contrast to other functions, this doesn't advance to next argument when
	 * argument is not NULL.
	 * @param iter iterator
	 * @param name name of argument
	 * @retval true argument is NULL, advanced to next argument
	 * @retval false argument is not NULL, not advanced - you have to read value
	 */
	bool isNull(ArgIter &iter, ConstStrA name);
	ConstStrA argStrA(ArgIter &iter, ConstStrA name, ConstStrA defval);
	ConstStrW argStrW(ArgIter &iter, ConstStrA name, ConstStrW defval);
	integer argInt(ArgIter &iter, ConstStrA name, integer defval);
	natural argUInt(ArgIter &iter, ConstStrA name, natural defval);
	double argFloat(ArgIter &iter, ConstStrA name, double defval);
	bool argBool(ArgIter &iter, ConstStrA name, bool defval);
	JSON::PNode argObject(ArgIter &iter, ConstStrA name, JSON::PNode defval);
	bool hasValue(ArgIter &iter, ConstStrA name);

	JSON::PFactory::Builder object() const {return JSON::PFactory(jsonFactory.get()).object();}
	JSON::PFactory::Builder array() const {return JSON::PFactory(jsonFactory.get()).array();}
	JSON::PNode ok() const {return jsonFactory->newValue(true);}

};


typedef Message<JSON::PNode,  RpcRequest *> RpcCall;
typedef RpcCall::Ifc IRpcCall;


class RpcError: public Exception {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcError(const ProgramLocation &loc, JSON::PNode errorNode);
	RpcError(const ProgramLocation &loc, JSON::IFactory *factory, natural status, String statusMessage);
	RpcError(const ProgramLocation &loc,const  RpcRequest *r, natural status, String statusMessage);
	~RpcError() throw() {}
	const JSON::PNode &getError() const;

	void message(ExceptionMsg &msg) const;

protected:
	JSON::PNode errorNode;
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


inline RpcError::RpcError(const ProgramLocation& loc,JSON::PNode errorNode)
	:Exception(loc),errorNode(errorNode)
{
}

inline RpcError::RpcError(const ProgramLocation &loc, JSON::IFactory *factory, natural status, String statusMessage)
	:Exception(loc),
	 errorNode(factory->newClass()->add("status",
			 	 factory->newValue(status))->add("statusMessage",
			 			 factory->newValue(statusMessage)))
{

}

inline RpcError::RpcError(const ProgramLocation &loc,const  RpcRequest *r, natural status, String statusMessage)
	:Exception(loc),
	 errorNode(r->jsonFactory->newClass()->add("status",
			 	 r->jsonFactory->newValue(status))->add("statusMessage",
			 			 r->jsonFactory->newValue(statusMessage)))
{

}


inline const JSON::PNode& RpcError::getError() const {
	return errorNode;
}

inline void RpcError::message(ExceptionMsg& msg) const {
	msg("JsonRpc Exception: %1") << JSON::createFast()->toString(*errorNode);
}

inline void RpcCallError::message(ExceptionMsg& msg) const {
	msg("JsonRpc Call Exception: %1 %2") << status << statusMessage;
}

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

