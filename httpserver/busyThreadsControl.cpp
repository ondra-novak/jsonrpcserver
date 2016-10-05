/*
 * busyThreadsControl.cpp
 *
 *  Created on: 5. 10. 2016
 *      Author: ondra
 */

#include "busyThreadsControl.h"

namespace BredyHttpSrv {


BredyHttpSrv::BusyThreadsControl::BusyThreadsControl(atomicValue maxLocks)
	:semaphore(maxLocks)
{

}

void BredyHttpSrv::BusyThreadsControl::unlock(LockStatus& status) {
	if (status.counter == 0) {
		semaphore.unlock();
	}
	status.counter++;
}


void BredyHttpSrv::BusyThreadsControl::lock(LockStatus& status) {
	if (status.counter) {
		if (--status.counter == 0) {
			semaphore.lock();
		}
	}
}

void BusyThreadsControl::lock() {
	semaphore.lock();
}

void BusyThreadsControl::unlock() {
	semaphore.unlock();
}

} /* namespace BredyHttpSrv */
