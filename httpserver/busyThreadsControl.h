/*
 * busyThreadsControl.h
 *
 *  Created on: 5. 10. 2016
 *      Author: ondra
 */

#ifndef BUSYTHREADSCONTROL_H_
#define BUSYTHREADSCONTROL_H_

#include <lightspeed/base/memory/refCntPtr.h>
#include <lightspeed/mt/linux/atomic_type.h>

#include "lightspeed/mt/semaphore.h"
namespace BredyHttpSrv {

using namespace LightSpeed;

class BusyThreadsControl: public LightSpeed::RefCntObj {
public:

	class LockStatus {
	public:
		LockStatus():counter(0) {}

		friend class BusyThreadsControl;

	protected:
		natural counter;
	};

	BusyThreadsControl(atomicValue maxLocks);
	void lock(LockStatus &status);
	void unlock(LockStatus &status);
	void lock();
	void unlock();

protected:
	Semaphore semaphore;
};

typedef RefCntPtr<BusyThreadsControl> PBusyThreadsControl;




} /* namespace BredyHttpSrv */

namespace LightSpeed{

template<>
class Synchronized<BredyHttpSrv::PBusyThreadsControl> {
public:

	Synchronized(BredyHttpSrv::PBusyThreadsControl semaphore):semaphore(semaphore) {
		semaphore->lock();
	}
	~Synchronized() {
		semaphore->unlock();
	}

protected:
	BredyHttpSrv::PBusyThreadsControl semaphore;
};

template<>
class SyncReleased<BredyHttpSrv::PBusyThreadsControl> {
public:

	SyncReleased(BredyHttpSrv::PBusyThreadsControl semaphore):semaphore(semaphore) {
		semaphore->unlock();
	}
	~SyncReleased() {
		semaphore->lock();
	}

protected:
	BredyHttpSrv::PBusyThreadsControl semaphore;
};

}

#endif /* BUSYTHREADSCONTROL_H_ */
