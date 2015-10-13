/*
 * stats.h
 *
 *  Created on: 11.10.2015
 *      Author: ondra
 */

#ifndef BREDYHTTPSERVER_STATS_H_
#define BREDYHTTPSERVER_STATS_H_
#include "../httpserver/httprequest.h"
#include <lightspeed/utils/json/json.h>
#include "lightspeed/utils/FilePath.h"

namespace BredyHttpSrv {

	class IJobScheduler;
	class HttpServer;


	class StatHandler: public IHttpHandler {
	public:
		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath);
		virtual natural onData(IHttpRequest &request);


		static JSON::PNode getStatsJSON(IHttpRequest *rq,JSON::IFactory *jsonFactory);

		void dumpStatsToFile(FilePath fname, HttpServer *server);
		void stopDumpJob();

		StatHandler();
		~StatHandler();
	protected:
		void *dumpJob;
		IJobScheduler* scheduler;

		struct DumpArgs {
			HttpServer *server;
			FilePath dumpPath;
			time_t lasttm;
			natural lastclk;
			DumpArgs(HttpServer *server,FilePath dumpPath)
				:server(server),dumpPath(dumpPath) {}
		};

		void dumpNow(DumpArgs args);
		void scheduleDump(DumpArgs args);
};

}


#endif /* BREDYHTTPSERVER_STATS_H_ */
