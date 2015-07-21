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

	enum Type {
		///create thread for scheduled task - may allocate additional resources
		newThread,
		///use thread from server thread pool - may reduce number of available threads for HTTP requests
		serverThread,
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
	 * @param action action to carry out at given time
	 * @param timeInS time in seconds measured from now. The scheduler has
	 * 		resolution in seconds, you cannot schedule to shorter interval
	 * @param threadMode choose which thread will be used to execute the job
	 * @return function returns a pointer, which can be later use to cancel the scheduled action
	 *
	 */
	virtual void *schedule(const LightSpeed::IThreadFunction &action, LightSpeed::natural timeInS,
			ThreadMode::Type threadMode = ThreadMode::newThread) = 0;

	virtual void *schedule(const LightSpeed::IThreadFunction &action,
						  const LightSpeed::IThreadFunction &rejectAction,
						  LightSpeed::natural timeInS,
						  ThreadMode::Type threadMode = ThreadMode::newThread) = 0;

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

};

///Allows to start background tasks on the server
/**Interface allows only to start a new job. It is on caller to
 * setup a communication with the newly created job.
 *
 *
 */
class IJobManager {
public:

	virtual void runJob(const LightSpeed::IThreadFunction &action, ThreadMode::Type threadMode = ThreadMode::newThread) = 0;

};

} /* namespace BredyHttpSrv */
#endif /* BREDYHTTPSERVER_IJOBSCHEDULER_H_ */
