/*
 * rpcerror.cpp
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#include "rpcerror.h"
#include "rpchandler.h"

namespace jsonsrv {

RpcError::RpcError(const ProgramLocation& loc,JSON::Value errorNode)
	:Exception(loc),errorNode(errorNode)
{
}

RpcError::RpcError(const ProgramLocation &loc, JSON::IFactory *factory, natural status, String statusMessage)
	:Exception(loc),
	 errorNode(factory->newClass()->add("status",
			 	 factory->newValue(status))->add("statusMessage",
			 			 factory->newValue(statusMessage)))
{

}

RpcError::RpcError(const ProgramLocation &loc,const  RpcRequest *r, natural status, String statusMessage)
	:Exception(loc),
	 errorNode(r->jsonFactory->newClass()->add("status",
			 	 r->jsonFactory->newValue(status))->add("statusMessage",
			 			 r->jsonFactory->newValue(statusMessage)))
{

}


const JSON::Value& RpcError::getError() const {
	return errorNode;
}

void RpcError::message(ExceptionMsg& msg) const {
	msg("JsonRpc Exception: %1") << JSON::createFast()->toString(*errorNode);
}

void RpcCallError::message(ExceptionMsg& msg) const {
	msg("JsonRpc Call Exception: %1 %2") << status << statusMessage;
}


}


