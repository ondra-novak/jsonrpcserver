/*
 * json.h
 *
 *  Created on: 31. 10. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_JSON_H_
#define JSONRPCSERVER_JSONRPC_JSON_H_

#include <imtjson/json.h>
#include "lightspeed/base/containers/string.h"
#include <lightspeed/base/text/textOut.h>
namespace jsonrpc {


typedef json::Value JValue;
typedef json::String JString;
typedef json::Object JObject;
typedef json::Array JArray;
using json::StrViewA;
using json::BinaryView;
using json::StrViewW;

///Contains string compatible with json::StringView and also LightSpeed::FlatArray which can be used as ConstStrA
/** class StrViewA augments json::StringView with operations from LightSpeed including iterators.
 * It also makes string acceptable by many functions which accepts ConstStrA
 */
/*class StrViewA: public json::StringView<char>, public LightSpeed::FlatArrayBase<const char, StrViewA> {
public:

	///main super class is json::StringView
	typedef json::StringView<char> Super;
	using Super::empty;
	using Super::operator[];
	using Super::StringView;

	StrViewA() {}
	StrViewA(const json::String &x):Super(x) {}
	StrViewA(const StringView<char> &other):json::StringView<char>(other) {}
	template<typename Impl>
	StrViewA(const LightSpeed::FlatArray<char, Impl> &other)
		:json::StringView<char>(other.data(),other.length()) {}
	template<typename Impl>
	StrViewA(const LightSpeed::FlatArray<const char, Impl> &other)
		:json::StringView<char>(other.data(),other.length()) {}

	explicit operator std::string () const {
		return std::string(Super::data, Super::length);
	}
//	operator json::String() const {return json::String(*this);}
	const char *data() const {return Super::data;}
	LightSpeed::natural length() const {return Super::length;}
	using json::StringView<char>::split;
};


*/

template<typename T>
static inline json::StringView<T> convStr(const LightSpeed::ConstStringT<T> &x) {
	return json::StringView<T>(x.data(),x.length());
}
template<typename T>
static inline json::StringView<T> convStr(const LightSpeed::StringCore<T> &x) {
	return json::StringView<T>(x.data(),x.length());
}
template<typename T>
static inline LightSpeed::ConstStringT<T> convStr(const json::StringView<T> &x) {
	return LightSpeed::ConstStringT<T>(x.data,x.length);
}

static inline LightSpeed::ConstStringT<char> convStr(const json::String&x) {
	json::StringView<char> y = x;
	return convStr(y);
}

}

#endif /* JSONRPCSERVER_JSONRPC_JSON_H_ */
