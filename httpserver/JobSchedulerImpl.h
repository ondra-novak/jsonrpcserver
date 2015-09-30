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

class JobSchedulerImpl: public IJobScheduler, public LightSpeed::OldScheduler {
public:
	JobSchedulerImpl(LightSpeed::IExecutor &serverExecutor);
	~JobSchedulerImpl();

	virtual void *schedule(const LightSpeed::IThreadFunction &action, LightSpeed::natural timeInS,
							ThreadMode::Type threadMode = ThreadMode::newThread) ;
	virtual void *schedule(const LightSpeed::IThreadFunction &action,
							const LightSpeed::IThreadFunction &rejectAction,
							LightSpeed::natural timeInS,
							ThreadMode::Type threadMode = ThreadMode::newThread ) ;
	virtual void cancel(void *action, bool async);

	virtual void notify();

	virtual LightSpeed::natural getCurTime() const;
protected:

	LightSpeed::PoolAlloc msgAlloc;
	LightSpeed::DirectExecutor directExecutor;
	LightSpeed::InfParallelExecutor infExecutor;
	LightSpeed::IExecutor &serverExecutor;
	LightSpeed::Thread worker;


	void workThread() throw();

protected:
	IExecutor* chooseExecutor(ThreadMode::Type threadMode);
	time_t startTime;
};

} /* namespace BredyHttpSrv */
#endif /* BREDYHTTPSERVER_JOBSCHEDULERIMPL_H_ */
