/*
 * headerValue.h
 *
 *  Created on: 19. 10. 2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_BREDY_HTTPSERVER_HEADERVALUE_H_
#define JSONRPCSERVER_BREDY_HTTPSERVER_HEADERVALUE_H_
#include "lightspeed/base/containers/constStr.h"

namespace BredyHttpSrv {


using namespace LightSpeed;

	class HeaderValue: public ConstStrA {
	public:
		const bool defined;
		HeaderValue():defined(false) {}
		HeaderValue(ConstStrA value):ConstStrA(value),defined(true) {}
	};

}



#endif /* JSONRPCSERVER_BREDY_HTTPSERVER_HEADERVALUE_H_ */
