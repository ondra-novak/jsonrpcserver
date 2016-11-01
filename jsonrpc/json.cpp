/*
 * json.cpp
 *
 *  Created on: 31. 10. 2016
 *      Author: ondra
 */

#include "json.h"

#include "lightspeed/base/containers/constStr.h"
namespace jsonrpc {

static json::PValue prepareArray(const std::initializer_list<JValue>& list) {
	JArray arr;
	arr.reserve(list.size());
	for(auto &&v :list) {
		arr.add(v);
	}
	return arr.commit();
}

JValue::JValue(const std::initializer_list<JValue>& list):JValue(prepareArray(list)) {

}

JValue::JValue(const json::Value& v):json::Value(v) {
}


LightSpeed::ConstStrA JValue::getStringA() const {
	auto x = JValue::getString();
	return LightSpeed::ConstStrA(x.data,x.length);
}

JValue JValue::operator [](const LightSpeed::ConstStrA& key)
{
	return JValue::operator[](json::StringView<char>(key.data(),key.length()));
}

LightSpeed::ConstStrA JValue::getKeyA() const {
	json::StringView<char> ret = JValue::getKey();
	return LightSpeed::ConstStrA(ret.data, ret.length);
}


JValue JValue::setKey(const LightSpeed::ConstStrA& key) const {
	return JValue::setKey(json::StringView<char>(key.data(),key.length()));
}

JValue JValue::fromString(const LightSpeed::ConstStrA& string) {
	return json::Value::fromString(json::StringView<char>(string.data(),string.length()));
}

JString::JString(const LightSpeed::ConstStrA& str):json::String(json::StringView<char>(str.data(),str.length())) {
}

JString::operator LightSpeed::ConstStrA() const {
	json::StringView<char> ret = (*this);
	return LightSpeed::ConstStrA(ret.data, ret.length);
}

JString::JString(const json::String& other):json::String(other) {
}

LightSpeed::ConstStrA JString::strA() const {
	json::StringView<char> ret = this->str();
	return LightSpeed::ConstStrA(ret.data, ret.length);
}


JObject::JObject(const LightSpeed::ConstStrA& name, const JValue& value) {
	set(name,value);
}

JObject::JObject(const json::Object& other):json::Object(other) {
}

JObject& JObject::set(const LightSpeed::ConstStrA& name,
					const JValue& value) {
	JObject::set(json::StringView<char>(name.data(),name.length()));
	return *this;
}

JValue JObject::operator [](const LightSpeed::ConstStrA& name) const {
	return this->operator[](json::StringView<char>(name.data(),name.length()));
}

JObject &JObject::unset(const LightSpeed::ConstStrA& name)  {
	JObject::unset(json::StringView<char>(name.data(),name.length()));
	return *this;
}

JObject& JObject::operator ()(const LightSpeed::ConstStrA& name,
		const JValue& value) {
}

JObject2Object JObject::object(const LightSpeed::ConstStrA& name) {
}

JArray2Object JObject::array(const LightSpeed::ConstStrA& name) {
}

}

