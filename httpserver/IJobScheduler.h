/*
 * IJobScheduler.h
 *
 *  Created on: Nov 4, 2012
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_IJOBSCHEDULER_H_
#define BREDYHTTPSERVER_IJOBSCHEDULER_H_
#include "lightspeed/mt/thread.h"

namespace BredyHttpSrv {


namespace ThreadMode {

	///deprecated
	enum Type {
		///run action in scheduler thread - may block the scheduler and delay other tasks
		schedulerThread

	};

}

///Server scheduler
/**
 * Scheduler is useful to schedule server jobs, or run server jobs at background
 *
 * Scheduler has resolution in seconds.
 *
 * To access scheduler, use getIfc<IJobScheduler> on IHttpServer and IHttpMapper. You don't need to
 * care about cancellation of scheduled jobs during destruction, because scheduler is terminated
 * before handlers are destroyed and after tcp port is closed (server thread pool is still available).
 *  Scheduler finalizes current message and then exits.
 */
class IJobScheduler {
public:


	///Schedules action to given time.
	/**
	 * @note function is deprecated
	 *
	 */
	virtual void *schedule(const LightSpeed::IThreadFunction &action, LightSpeed::natural timeInS,
			ThreadMode::Type ) {
		return schedule(action,timeInS);
	}

	///Schedules action to a given time
	/**
	 * @param action reference to action created using ThreadFunction::create(). You can
	 * also use Action::create() because ThreadFunction is compatible with the Action. Also
	 * any instance of Action is accepted. Note that function will copy the action.
	 *
	 * @param timeInS time in seconds. You cannot use less resolution than 1 second, it is by design.
	 * To schedule action for less than 1 second, create extra thread.
	 *
	 * @note Actions are executed in thread of the scheduler. Action should exit as soon
	 * as possible. For long task, action can use IJobManager to start new jobs (however
	 * action will need to carry the reference to the IJobManager to access job manager, because
	 * internal scheduler doesn't implement it.
	 *
	 * Action can reschedule itself to create a loop. It should do this in the scheduler's thread
	 * before it spawns a new job, otherwise it is impossible to safely stop such a loop (due
	 * various race conditions)
	 *
	 *
	 *
	 */
	virtual void *schedule(const LightSpeed::IThreadFunction &action, LightSpeed::natural timeInS) = 0;


	///Deprecated function
	virtual void *schedule(const LightSpeed::IThreadFunction &action,
						  const LightSpeed::IThreadFunction &rejectAction,
						  LightSpeed::natural timeInS,
						  ThreadMode::Type) {
		return schedule(action,rejectAction,timeInS);
	}

	///Schedules action at a given time
	/**
	 * @param action reference to action created using ThreadFunction::create(). You can
	 * also use Action::create() because ThreadFunction is compatible with the Action. Also
	 * any instance of Action is accepted. Note that function will copy the action.
	 *
	 * @rejectAction function called when server exits before action is executed
	 *
	 * @param timeInS time in seconds. You cannot use less resolution than 1 second, it is by design.
	 * To schedule action for less than 1 second, create extra thread.
	 *
	 * @note Actions are executed in thread of the scheduler. Action should exit as soon
	 * as possible. For long task, action can use IJobManager to start new jobs (however
	 * action will need to carry the reference to the IJobManager to access job manager, because
	 * internal scheduler doesn't implement it.
	 *
	 * Action can reschedule itself to create a loop. It should do this in the scheduler's thread
	 * before it spawns a new job, otherwise it is impossible to safely stop such a loop (due
	 * various race conditions)
	 *
	 */
	virtual void *schedule(const LightSpeed::IThreadFunction &action,
						  const LightSpeed::IThreadFunction &rejectAction,
						  LightSpeed::natural timeInS) = 0;

	///Cancels scheduled action
	/**
	 * @param action pointer to scheduled action returned by function schedule
	 * @param async true to perform asynchronous cancellation. In this case, cancellation request is
	 *  queued to the scheduler and carried as priority message. Scheduler may already been processing
	 *  message which can be subject of cancellation. In asynchronous mode, function will continue even
	 *  if message has not been canceled because it is in progress. Specify false to perform synchronous request.
	 *  Function places cancellation order and blocks until it is carried.
	 */
	virtual void cancel(void *action, bool async) = 0;

	///Cancels loop
	/**Function safely cancels scheduled loop - scheduled action which reschedules
	 * itself on each run. Function monitors action variable during cancel and if
	 * rescheduling happened, it repeats operation, until the action is canceled
	 *
	 * @param action reference to action pointer. Function expects, that every cycle the
	 * scheduled action stores
	 *
	 */
	virtual void cancelLoop( void * volatile &action) = 0;

	virtual ~IJobScheduler() {}

};

///Allows to start background tasks on the server
/**Interface allows only to start a new job. It is on caller to
 * setup a communication with the newly created job.
 *
 * Job interface implemented in HttpServer allow to spawn new job using the thread from
 * the server's pool. You can access this job manager using getIfc<IJobManager> on IHttpRequest.
 * Because threads are limited resource, you should always be aware on
 * how many jobs can run at time. Every running job decreases count of available threads in
 * server's thread pool for the requests.
 *
 * Job interface implemented through the scheduler will use scheduler's thread. You
 * can access this interface using getIfc<IJobManager> on IJobScheduler.
 *
 */
class IJobManager {
public:

	///Deprecated call
	virtual void runJob(const LightSpeed::IThreadFunction &action, ThreadMode::Type) {
		runJob(action);
	}
	///Executes job.
	virtual void runJob(const LightSpeed::IThreadFunction &action) = 0;

};

} /* namespace BredyHttpSrv */
#endif /* BREDYHTTPSERVER_IJOBSCHEDULER_H_ */
