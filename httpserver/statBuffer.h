/*
 * StatBuffer.h
 *
 *  Created on: 19.6.2013
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_STATBUFFER_H_
#define BREDYHTTPSERVER_STATBUFFER_H_
#include "lightspeed/base/types.h"
#include <time.h>
#include "lightspeed/base/sync/synchronize.h"
#include "lightspeed/mt/fastlock.h"
#include "lightspeed/base/containers/optional.h"

namespace BredyHttpSrv {

template<typename T, LightSpeed::natural length = 310>
class StatBuffer {
public:

	StatBuffer();
	void add(T value);
	T getAvg(LightSpeed::natural l) const;
	T getMax(LightSpeed::natural l) const;
	T getMin(LightSpeed::natural l) const;
	static LightSpeed::natural getIndex();
	T getSum() const {return sum;}


protected:

	T data[length];
	T sum;
	LightSpeed::natural lastIndex;
	mutable LightSpeed::FastLock lk;
};

class StatAvg {
public:
	StatAvg(double val);
	StatAvg(LightSpeed::NullType x);
	StatAvg();

	StatAvg &operator+=(const StatAvg &other);
	StatAvg &operator/=(LightSpeed::natural ) {return *this;}
	double getValue() const;
	bool isDefined() const;
	double getMin() const {return minval;}
	double getMax() const {return maxval;}
protected:
	double value;
	double minval;
	double maxval;
	LightSpeed::natural count;

};

template<typename T>
class StatValue {
public:
	StatValue(T val);
	StatValue(LightSpeed::NullType x);
	StatValue();
	StatValue &operator+=(const StatValue &other);
	StatValue &operator/=(LightSpeed::natural count);
	T getValue() const;
	bool isDefined() const;
protected:
	T value;
};

}

namespace std {
inline const BredyHttpSrv::StatAvg max(const BredyHttpSrv::StatAvg &a, const BredyHttpSrv::StatAvg &b) {
	if (!a.isDefined()) return b;
	if (!b.isDefined()) return a;
	return a.getMax() > b.getMax()?a.getMax():b.getMax();
}

inline const BredyHttpSrv::StatAvg min(const BredyHttpSrv::StatAvg &a, const BredyHttpSrv::StatAvg &b) {
	if (!a.isDefined()) return b;
	if (!b.isDefined()) return a;
	return a.getMin() < b.getMin()?a.getMin():b.getMin();
}

template<typename T>
inline const BredyHttpSrv::StatValue<T> &max(const BredyHttpSrv::StatValue<T> &a, const BredyHttpSrv::StatValue<T> &b) {
	return a.getValue() > b.getValue()?a:b;
}

template<typename T>
inline const BredyHttpSrv::StatValue<T> &min(const BredyHttpSrv::StatValue<T> &a, const BredyHttpSrv::StatValue<T> &b) {
	return a.getValue() < b.getValue()?a:b;
}
}

namespace BredyHttpSrv{

template<typename T, LightSpeed::natural length>
inline void StatBuffer<T,length>::add(T value) {
	using namespace LightSpeed;
	Synchronized<FastLock> _(lk);
	natural idx = getIndex();
	while (idx != lastIndex) {
		lastIndex = (lastIndex+1) % length;
		data[lastIndex] = nil;
	}
	data[idx]+=(value);
	sum+=(value);
}


template<typename T, LightSpeed::natural length >
inline T StatBuffer<T,length>::getAvg(LightSpeed::natural l) const {
	using namespace LightSpeed;
	natural idx = lastIndex;
	T sum = nil;
	for (natural i = 0; i < l; i++) {
		idx--;
		if (idx == naturalNull) idx+=length;
		sum += data[idx];
	}
	sum/=l;
	return sum;
}

template<typename T, LightSpeed::natural length >
inline T StatBuffer<T,length>::getMax(LightSpeed::natural l) const {
	using namespace LightSpeed;
	natural idx = lastIndex;
	idx --;
	if (idx == naturalNull) idx+=length;
	T sum = data[idx];
	for (natural i = 1; i < l; i++) {
		idx--;
		if (idx == naturalNull) idx+=length;
		sum = std::max(sum, data[idx]);

	}
	return sum;
}

template<typename T, LightSpeed::natural length>
inline T StatBuffer<T,length>::getMin(LightSpeed::natural l) const {
	using namespace LightSpeed;
	natural idx = lastIndex;
	idx --;
	if (idx == naturalNull) idx+=length;
	T sum = data[idx];
	for (natural i = 1; i < l; i++) {
		idx--;
		if (idx == naturalNull) idx+=length;
		sum = std::min(sum, data[idx]);
	}
	return sum;
}

template<typename T, LightSpeed::natural length>
inline LightSpeed::natural StatBuffer<T,length>::getIndex() {
	using namespace LightSpeed;
	time_t tm;
	time(&tm);
	return tm % length;
}

template<typename T, LightSpeed::natural length>
inline StatBuffer<T,length>::StatBuffer() {
	using namespace LightSpeed;
	for (natural i = 0; i < length; i++) data[i] = nil;
	lastIndex = 0;
}


template<typename T>
inline BredyHttpSrv::StatValue<T>::StatValue(T val):value(val) {
}

template<typename T>
inline BredyHttpSrv::StatValue<T>::StatValue(LightSpeed::NullType ):value(T(LightSpeed::nil)) {
}

template<typename T>
inline BredyHttpSrv::StatValue<T>::StatValue():value(0) {
}

template<typename T>
inline StatValue<T>& BredyHttpSrv::StatValue<T>::operator +=(const StatValue& other) {
	value +=other.value;
	return *this;
}

template<typename T>
inline T BredyHttpSrv::StatValue<T>::getValue() const {
	return value;
}

template<typename T>
inline bool BredyHttpSrv::StatValue<T>::isDefined() const {
	return true;
}

template<typename T>
inline StatValue<T>& BredyHttpSrv::StatValue<T>::operator /=(
		LightSpeed::natural count) {
	value/=count;
	return *this;
}

} /* namespace BredyHttpSrv */


#endif /* BREDYHTTPSERVER_STATBUFFER_H_ */
