/*
 * json.h
 *
 *  Created on: 31. 10. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_JSON_H_
#define JSONRPCSERVER_JSONRPC_JSON_H_

#include <immujson/json.h>
#include "lightspeed/base/containers/string.h"
namespace jsonrpc {


typedef json::Value JValue;
typedef json::String JString;
typedef json::Object JObject;
typedef json::Array JArray;

}


template<typename T>
static inline json::StringView<T> operator~(const LightSpeed::ConstStringT<T> &x) {
	return json::StringView<T>(x.data(),x.length());
}
template<typename T>
static inline json::StringView<T> operator~(const LightSpeed::StringCore<T> &x) {
	return json::StringView<T>(x.data(),x.length());
}
template<typename T>
static inline LightSpeed::ConstStringT<T> operator~(const json::StringView<T> &x) {
	return LightSpeed::ConstStringT<T>(x.data,x.length);
}

static inline LightSpeed::ConstStringT<char> operator~(const json::String&x) {
	json::StringView<char> y = x;
	return ~y;
}



#endif /* JSONRPCSERVER_JSONRPC_JSON_H_ */
