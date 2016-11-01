/*
 * json.h
 *
 *  Created on: 31. 10. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_JSON_H_
#define JSONRPCSERVER_JSONRPC_JSON_H_

#include <immujson/json.h>
#include "lightspeed/base/containers/constStr.h"

namespace jsonrpc {


class JValue: public json::Value {
public:
	using json::Value::Value;
	using json::Value::operator[];
	using json::Value::setKey;
	JValue() {}
	JValue(const Value &v);
	JValue(const LightSpeed::ConstStrA &x)
		:JValue(json::StringView<char>(x.data(),x.length())) {};
	JValue(const std::initializer_list<JValue> &list);
	LightSpeed::ConstStrA getStringA() const;
	JValue operator[](const LightSpeed::ConstStrA &key);
	LightSpeed::ConstStrA getKeyA() const;
	JValue setKey(const LightSpeed::ConstStrA &key) const;
	static JValue fromString(const LightSpeed::ConstStrA &string);

};

class JString: public json::String {
public:
	using json::String::String;
	JString() {}
	JString(const LightSpeed::ConstStrA &str);
	JString(const json::String &other);

	operator LightSpeed::ConstStrA () const;
	LightSpeed::ConstStrA strA() const;
};

class JObject;
class JArray;
typedef json::AutoCommitT<JObject, JObject, LightSpeed::ConstStrA > JObject2Object;
typedef json::AutoCommitT<JObject, JArray, std::size_t > JObject2Array;
typedef json::AutoCommitT<JArray, JObject, LightSpeed::ConstStrA >  JArray2Object ;
typedef json::AutoCommitT<JArray, JArray, std::size_t > JArray2Array;


class JObject: public json::Object {
public:
	using json::Object::Object;
	JObject() {}
	JObject(const LightSpeed::ConstStrA &name, const JValue &value);
	JObject(const char *name, const JValue &value);
	JObject(const json::Object &other);
	using json::Object::set;
	using json::Object::operator[];
	using json::Object::operator();
	using json::Object::unset;
	JObject &set(const LightSpeed::ConstStrA &name, const JValue &value);
	JObject &set(const char *name, const JValue &value);
	JValue operator[](const LightSpeed::ConstStrA &name) const;
	JObject &unset(const LightSpeed::ConstStrA &name);
	JObject &operator()(const LightSpeed::ConstStrA &name, const JValue &value);
	JObject &operator()(const char *name, const JValue &value);

	using json::Object::object;
	using json::Object::array;
	JObject2Object object(const LightSpeed::ConstStrA &name);
	JArray2Object array(const LightSpeed::ConstStrA &name);


};
class JArray: public json::Array {
public:
	JArray() {}
	using json::Array::Array;
};





}




#endif /* JSONRPCSERVER_JSONRPC_JSON_H_ */
