/*
 * errors.cpp
 *
 *  Created on: 10. 8. 2016
 *      Author: ondra
 */


#include "errors.h"

#include <lightspeed/utils/json/jsonserializer.tcc>
#include "lightspeed/base/containers/autoArray.tcc"

#include "lightspeed/base/memory/smallAlloc.h"

using LightSpeed::JSON::Serializer;
using LightSpeed::SmallAlloc;
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


void ParseException::message(ExceptionMsg& msg) const {
}

JSON::ConstValue ParseException::getJSON(
		const JSON::Builder& json) const {
}

UncauchException::UncauchException(const ProgramLocation& loc,
		const Exception& e):Exception(loc)
		,MethodException(loc,500,e.getMessageWithReason().getUtf8())
		,type(typeid(e))

{
}

JSON::ConstValue UncauchException::getJSON(const JSON::Builder& json) const {
	return json("class","uncaughException")
			("status",statusCode)
			("statusMessage",statusMessage)
			("type",type.name());

}


RemoteException::RemoteException(const ProgramLocation& loc,
			const JSON::ConstValue& data):Exception(loc),data(data)

{
}

JSON::ConstValue RemoteException::getJSON(const JSON::Builder&) const {
	return data;
}

void RemoteException::message(ExceptionMsg& msg) const {
	AutoArrayStream<char, SmallAlloc<1024> > buffer;
	JSON::serialize(data, buffer, false);
	msg("Remote server exception: %1") << buffer.getArray();
}


}
