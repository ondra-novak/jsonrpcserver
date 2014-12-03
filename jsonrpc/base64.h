/*
 * base64.h
 *
 *  Created on: Dec 8, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_BASE64_H_
#define JSONRPCSERVER_BASE64_H_


#include "lightspeed/utils/base64.h"
#include "lightspeed/base/iter/iteratorFilter.tcc"
#include <lightspeed/base/containers/string.h>
#include "lightspeed/base/containers/autoArray.h"

namespace jsonsrv {

static inline LightSpeed::StringA base64_encode(LightSpeed::ConstStrA x) {
	using namespace LightSpeed;
	ConstStringT<byte> bx((const byte *)x.data(),x.length());
	AutoArrayStream<char> buffer;
	buffer.reserve((x.length() + 2 / 3) * 4);
	ConstStringT<byte>::Iterator srciter = bx.getFwIter();
	ReadAndEncodeBase64 < ConstStringT<byte>::Iterator > enc(srciter);
	buffer.copy(enc);
	return buffer.getArray();
}

static inline LightSpeed::StringA base64_decode(LightSpeed::ConstStrA x) {
	using namespace LightSpeed;
	AutoArrayStream<char> buffer;
	buffer.reserve((x.length() + 3 / 4) * 3);
	ConstStrA::Iterator srciter = x.getFwIter();
	ReadAndDecodeBase64 < ConstStrA::Iterator > enc(srciter);
	buffer.copy(enc);
	return buffer.getArray();
}

}


#endif /* JSONRPCSERVER_BASE64_H_ */
