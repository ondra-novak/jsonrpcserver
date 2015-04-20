/*
 * StatBuffer.cpp
 *
 *  Created on: 19.6.2013
 *      Author: ondra
 */

#include <algorithm>
#include "statBuffer.h"

namespace BredyHttpSrv {

using namespace LightSpeed;

#undef max
#undef min

StatAvg::StatAvg(double val):value(val),minval(val),maxval(val),count(1) {
}

StatAvg::StatAvg(LightSpeed::NullType):value(0),count(0) {
}

StatAvg& StatAvg::operator +=(const StatAvg& other) {
	if (count) {
		minval = std::min(minval,other.minval);
		maxval = std::max(maxval,other.maxval);
	} else {
		minval = other.minval;
		maxval = other.maxval;
	}
	value += other.value;
	count += other.count;

	return *this;
}

double StatAvg::getValue() const {
	return value/count;
}

StatAvg::StatAvg():value(0),count(0) {
}


bool StatAvg::isDefined() const {
	return count>0;
}

} /* namespace BredyHttpSrv */
