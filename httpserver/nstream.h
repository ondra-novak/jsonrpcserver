/*
 * nstream.h
 *
 *  Created on: Mar 9, 2015
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_NSTREAM_H_
#define BREDYHTTPSERVER_NSTREAM_H_
#include "lightspeed/base/streams/netio.h"

namespace BredyHttpSrv {

typedef LightSpeed::NetworkStream<8196> NStream;
}



#endif /* BREDYHTTPSERVER_NSTREAM_H_ */
