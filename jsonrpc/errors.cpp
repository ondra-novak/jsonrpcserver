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

JValue LookupException::getJSON() const {
	return JObject("class",JValue("lookupException"))
			("message",JValue("Method not found."))
			("method",JValue(StrView(prototype)));

}

void MethodException::message(ExceptionMsg& msg) const {
	msg("Rpc method exception: %1 %2") << statusCode << statusMessage;
}


JValue MethodException::getJSON() const {
	return JObject("class",JValue("methodException"))
			("status",JValue(statusCode))
			("statusMessage",JValue(StrView(statusMessage)));
}


void ParseException::message(ExceptionMsg& msg) const {
	msg("Parse exception: %1") << this->description;
}

JValue ParseException::getJSON() const {
	return JObject("class",JValue("ParseException"))
			("status",JValue(400))
			("statusMessage",JValue(StrView(description)));
}

UncauchException::UncauchException(const ProgramLocation& loc,
		const Exception& e):Exception(loc)
		,MethodException(loc,500,e.getMessageWithReason().getUtf8())
		,type(typeid(e))

{
}

JValue UncauchException::getJSON() const {
	return JObject("class",JValue("uncaughException"))
			("status",JValue(statusCode))
			("statusMessage",JValue(StrView(statusMessage)))
			("type",JValue(type.name()));

}


RemoteException::RemoteException(const ProgramLocation& loc,
			const JValue& data):Exception(loc),data(data)

{
}

JValue RemoteException::getJSON() const {
	return data;
}

void RemoteException::message(ExceptionMsg& msg) const {
	AutoArrayStream<char, SmallAlloc<1024> > buffer;
	data.serialize([&](char c) {buffer.write(c);});
	msg("Remote server exception: %1") << buffer.getArray();
}


}
