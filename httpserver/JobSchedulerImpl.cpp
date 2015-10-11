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
#include <time.h>
#include "lightspeed/base/exceptions/stdexception.h"
#include "lightspeed/base/framework/iapp.h"

namespace BredyHttpSrv {

using namespace LightSpeed;

JobSchedulerImpl::JobSchedulerImpl() {
	time(&startTime);
}


class ScheduledAction: public OldScheduler::AbstractEvent, public DynObject {
public:

	ScheduledAction(const LightSpeed::Action::Ifc &msg, IRuntimeAlloc &alloc);
	virtual void run() throw();

protected:

	LightSpeed::Action msg;

};

class ScheduledActionReject: public ScheduledAction {
public:

	ScheduledActionReject(const LightSpeed::Action::Ifc &msg, const LightSpeed::Action::Ifc &cancelMsg, IRuntimeAlloc &alloc);
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
	return  (natural)curTime - (natural)startTime;
}


void* JobSchedulerImpl::schedule(const IThreadFunction &action, natural timeInS ) {
	ScheduledAction* a = new ScheduledAction(action,msgAlloc);
	OldScheduler::schedule(a, timeInS);
	return a;
}

void JobSchedulerImpl::cancel(void* action, bool async) {
	ScheduledAction* a = (ScheduledAction*) ((((action))));
	OldScheduler::cancelMessage(a, async);
}

void JobSchedulerImpl::notify() {
	if (!worker.isRunning()) {
		worker.start(
				ThreadFunction::create(this, &JobSchedulerImpl::workThread));
	}
	worker.wakeUp(0);
}


void* JobSchedulerImpl::schedule(const LightSpeed::IThreadFunction &action,
		const LightSpeed::IThreadFunction &rejectAction, LightSpeed::natural timeInS ) {
	ScheduledActionReject* a = new ScheduledActionReject(Action(action),  Action(rejectAction),  msgAlloc);
	OldScheduler::schedule(a, timeInS);
	return a;
}

void JobSchedulerImpl::cancelLoop( void * volatile& action) {
	 void * volatile d;
	do {
		d = action;
		if (d == 0) break;
		cancel(d,false);
	} while (d != action);
}

void JobSchedulerImpl::runJob(const LightSpeed::IThreadFunction& action) {
	schedule(action,0);
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
			Thread::sleep(wt * 1000);
		} else {
			Thread::sleep (nil);
		}
	}
	lg.debug("Scheduler exit");
}

inline ScheduledAction::ScheduledAction(const LightSpeed::Action::Ifc &msg,
		IRuntimeAlloc& alloc) : msg(msg.clone(alloc))
{
}

inline void ScheduledAction::run() throw () {
	try {
		msg();
	} catch (::LightSpeed::Exception &e) {
		IApp::threadException(e);
	} catch (std::exception &e) {
		IApp::threadException(StdException(THISLOCATION, e));
	} catch (...) {
		IApp::threadException(UnknownException(THISLOCATION));
	}
}

inline void ScheduledActionReject::reject() throw () {
	rejectMsg();
}


inline ScheduledActionReject::ScheduledActionReject(const LightSpeed::Action::Ifc & msg,
		const LightSpeed::Action::Ifc & cancelMsg,  IRuntimeAlloc& alloc)
	:ScheduledAction(msg, alloc),rejectMsg(cancelMsg.clone(alloc))

{
}


}
