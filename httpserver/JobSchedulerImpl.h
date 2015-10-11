/*
 * JobSchedulerImpl.h
 *
 *  Created on: Nov 5, 2012
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_JOBSCHEDULERIMPL_H_
#define BREDYHTTPSERVER_JOBSCHEDULERIMPL_H_

#include "IJobScheduler.h"
#include "lightspeed/base/memory/poolalloc.h"
#include <lightspeed/base/actions/directExecutor.h>
#include <lightspeed/base/actions/schedulerOld.h>
#include "lightspeed/base/actions/InfParallelExecutor.h"

namespace BredyHttpSrv {

class JobSchedulerImpl: public IJobScheduler, public IJobManager, public LightSpeed::OldScheduler {
public:
	JobSchedulerImpl();
	~JobSchedulerImpl();

	virtual void *schedule(const LightSpeed::IThreadFunction &action, LightSpeed::natural timeInS) ;
	virtual void *schedule(const LightSpeed::IThreadFunction &action,
							const LightSpeed::IThreadFunction &rejectAction,
							LightSpeed::natural timeInS) ;
	virtual void cancel(void *action, bool async);

	virtual void cancelLoop( void * volatile &action);

	virtual void runJob(const LightSpeed::IThreadFunction &action);

	virtual void notify();

	virtual LightSpeed::natural getCurTime() const;
protected:

	LightSpeed::PoolAlloc msgAlloc;
	LightSpeed::Thread worker;


	void workThread() throw();

protected:
	time_t startTime;
};

} /* namespace BredyHttpSrv */
#endif /* BREDYHTTPSERVER_JOBSCHEDULERIMPL_H_ */
