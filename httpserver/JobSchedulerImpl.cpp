/*
 * JobSchedulerImpl.cpp
 *
 *  Created on: Nov 5, 2012
 *      Author: ondra
 */

#include "JobSchedulerImpl.h"
#include <lightspeed/base/actions/directExecutor.h>
#include "lightspeed/base/exceptions/invalidParamException.h"
#include "lightspeed/base/debug/dbglog.h"

namespace BredyHttpSrv {

using namespace LightSpeed;

JobSchedulerImpl::JobSchedulerImpl(LightSpeed::IExecutor &serverExecutor):serverExecutor(serverExecutor) {
	time(&startTime);
}


class ScheduledAction: public Scheduler::AbstractEvent, public DynObject {
public:

	ScheduledAction(const LightSpeed::Action::Ifc &msg, IExecutor *executor, IRuntimeAlloc &alloc);
	virtual void run() throw();

protected:

	IExecutor *executor;
	LightSpeed::Action msg;

};

class ScheduledActionReject: public ScheduledAction, public DynObject {
public:

	ScheduledActionReject(const LightSpeed::Action::Ifc &msg, const LightSpeed::Action::Ifc &cancelMsg, IExecutor *executor, IRuntimeAlloc &alloc);
	virtual void reject() throw ();

protected:

	LightSpeed::Action rejectMsg;

};

JobSchedulerImpl::~JobSchedulerImpl() {
	worker.stop();
	wtFlush();
}

LightSpeed::natural JobSchedulerImpl::getCurTime() const {
	time_t curTime;
	time(&curTime);
	return  curTime - startTime;
}

IExecutor* JobSchedulerImpl::chooseExecutor(ThreadMode::Type threadMode) {
	IExecutor* executor;
	switch (threadMode) {
	case ThreadMode::schedulerThread:
		executor = &directExecutor;
		break;
	case ThreadMode::newThread:
		executor = &infExecutor;
		break;
	case ThreadMode::serverThread:
		executor = &serverExecutor;
		break;
	default:
		throw InvalidParamException(THISLOCATION, 1, "Invalid thread mode");
	}
	return executor;
}

void* JobSchedulerImpl::schedule(const IThreadFunction &action, natural timeInS,
		ThreadMode::Type threadMode) {
	ScheduledAction* a = new ScheduledAction(Action(action), chooseExecutor(threadMode),
			msgAlloc);
	Scheduler::schedule(a, timeInS);
	return a;
}

void JobSchedulerImpl::cancel(void* action, bool async) {
	ScheduledAction* a = (ScheduledAction*) ((((action))));
	Scheduler::cancelMessage(a, async);
}

void JobSchedulerImpl::notify() {
	if (!worker.isRunning()) {
		worker.start(
				ThreadFunction::create(this, &JobSchedulerImpl::workThread));
	}
	worker.wakeUp(0);
}


void* JobSchedulerImpl::schedule(const LightSpeed::IThreadFunction &action,
		const LightSpeed::IThreadFunction &rejectAction, LightSpeed::natural timeInS,
		ThreadMode::Type threadMode) {
	ScheduledActionReject* a = new ScheduledActionReject(Action(action),  Action(rejectAction), chooseExecutor(threadMode), msgAlloc);
	Scheduler::schedule(a, timeInS);
	return a;
}

void JobSchedulerImpl::workThread() throw () {
	LogObject lg(THISLOCATION);
	DbgLog::setThreadName("scheduler",false);
	lg.debug("Scheduler started");
	while (!Thread::canFinish()) {
		natural wt = getWaitTime();
		if (wt == 0) {
			lg.debug("Processing scheduled action");
			pumpMessage();
		} else if (wt != naturalNull) {
			lg.debug("Sleeping for %1 seconds") << wt;
			Thread::sleep(wt * 1000);
		} else {
			lg.debug("Waiting for actions") << wt;
			Thread::sleep (nil);
		}
	}
	lg.debug("Scheduler exit");
}

inline ScheduledAction::ScheduledAction(const LightSpeed::Action::Ifc &msg, IExecutor* executor,
		IRuntimeAlloc& alloc) :
		executor(executor), msg(msg.clone(alloc))
{
}

inline void ScheduledAction::run() throw () {
	try {
		executor->execute(msg);
	} catch (std::exception &e) {
		LogObject(THISLOCATION).error("Exception in scheduler: %1") << e.what();
	}
}

inline void ScheduledActionReject::reject() throw () {
	rejectMsg();
}


inline ScheduledActionReject::ScheduledActionReject(const LightSpeed::Action::Ifc & msg,
		const LightSpeed::Action::Ifc & cancelMsg, IExecutor* executor, IRuntimeAlloc& alloc)
	:ScheduledAction(msg,executor, alloc),rejectMsg(cancelMsg.clone(alloc))

{
}


}
