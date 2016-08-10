/*
 * errors.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */


#include "errors.h"

namespace jsonrpc {

void LookupException::message(ExceptionMsg& msg) const {
	msg("Method not found: %1") << prototype;
}

JSON::ConstValue LookupException::getJSON( const JSON::Builder& json) const {
	return json("class","lookupException")
			("message","Method not found.")
			("method",prototype);

}

void MethodException::message(ExceptionMsg& msg) const {
	msg("Rpc method exception: %1 %2") << statusCode << statusMessage;
}


JSON::ConstValue MethodException::getJSON(const JSON::Builder& json) const {
	return json("class","methodException")
			("status",statusCode)
			("statusMessage",statusMessage);
}


}
