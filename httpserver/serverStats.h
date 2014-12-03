/*
 * serverStats.h
 *
 *  Created on: 19.6.2013
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_SERVERSTATS_H_
#define BREDYHTTPSERVER_SERVERSTATS_H_
#include <utility>
#include "statBuffer.h"


namespace BredyHttpSrv {

	struct HttpStats {

		StatBuffer<StatValue<double> > requests;
		StatBuffer<StatAvg> threads;
		StatBuffer<StatAvg> idleThreads;
		StatBuffer<StatAvg> connections;
		StatBuffer<StatAvg> latency;
		StatBuffer<StatValue<double> > worktime;

	};

}



#endif /* BREDYHTTPSERVER_SERVERSTATS_H_ */
