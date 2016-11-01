/*
 * stats.cpp
 *
 *  Created on: 11.10.2015
 *      Author: ondra
 */
#include "stats.h"
#include "lightspeed/base/streams/fileio.h"
#include "statBuffer.h"
#include <lightspeed/base/memory/staticAlloc.h>
#include "lightspeed/base/text/textFormat.tcc"
#include "../httpserver/httpServer.h"
#include "lightspeed/utils/json/jsonbuilder.h"
#include <lightspeed/base/framework/proginstance.h>
#include <lightspeed/base/text/textstream.tcc>
#include "lightspeed/base/interface.tcc"

using LightSpeed::StaticAlloc;
using LightSpeed::IInterface;

namespace BredyHttpSrv {

natural StatHandler::onRequest(IHttpRequest& request, ConstStrA ) {
	JSON::PFactory f = JSON::create();
	JSON::PNode x = getStatsJSON(&request,f);
	SeqFileOutput out(&request);
	request.header(IHttpRequest::fldContentType,"application/json");
	request.header(IHttpRequest::fldCacheControl,"no-cache, no-store, must-revalidate");
	request.header(IHttpRequest::fldExpires,"0");
	f->toStream(*x,out);
	return 0;
}

natural StatHandler::onData(IHttpRequest& ) {
	return 0;
}

namespace {
template<typename T>
JSON::PNode avgField( const StatBuffer<T> &b, JSON::IFactory *f, natural cnt )
{
	T c = b.getAvg(cnt);
	if (c.isDefined()) return f->newValue(c.getValue());
	else return f->newNullNode();
}

template<typename T>
JSON::PNode minField( const StatBuffer<T> &b, JSON::IFactory *f, natural cnt )
{
	T c = b.getMin(cnt);
	if (c.isDefined()) return f->newValue(c.getValue());
	else return f->newNullNode();
}

template<typename T>
JSON::PNode maxField( const StatBuffer<T> &b, JSON::IFactory *f, natural cnt )
{
	T c = b.getMax(cnt);
	if (c.isDefined()) return f->newValue(c.getValue());
	else return f->newNullNode();
}

template<typename T>
JSON::PNode statFields( const StatBuffer<T> &b, JSON::IFactory *f )
{
	/*
	JSON::Builder_del json(f);
	return json
			("avg006",avgField(b,f,6))
			("avg030",avgField(b,f,30))
			("avg060",avgField(b,f,60))
			("avg300",avgField(b,f,300))
			("min006",minField(b,f,6))
			("min030",minField(b,f,30))
			("min060",minField(b,f,60))
			("min300",minField(b,f,300))
			("max006",maxField(b,f,6))
			("max030",maxField(b,f,30))
			("max060",maxField(b,f,60))
			("max300",maxField(b,f,300));
*/
}


AutoArray<char,StaticAlloc<19> > getTimeAsStr(natural seconds) {
	TextFormatBuff<char, StaticAlloc<19> > out;
	out("%1d, %{02}2:%{02}3:%{02}4")
		<< seconds / 86400
		<< (seconds % 86400)/3600
		<< (seconds % 3600)/60
		<< (seconds % 60);
	return out.write();
}
}

JSON::PNode BredyHttpSrv::StatHandler::getStatsJSON(
		IHttpRequestInfo *rq,
		JSON::IFactory* f) {
	/*
	const HttpServer &server = rq->getIfc<HttpServer>();
	const HttpStats &st = server.getStats();
	JSON::Builder_del json(f);
	JSON::Builder_del::Object out = json.object();
	out("request", statFields(st.requests,f))
		("threads", statFields(st.threads,f))
		("threadsIdle", statFields(st.idleThreads,f))
		("connections", statFields(st.connections,f))
		("latency", statFields(st.latency,f))
		("worktime", statFields(st.worktime,f));

	TimeStamp waketime = TimeStamp::fromUnix(ProgInstance::getUpTime(false));

		out("crashCount",ProgInstance::getRestartCounts());
		out("crashesPerDay", ProgInstance::getRestartCounts()/waketime.getFloat());
		out("upTime",json("total",ConstStrA(getTimeAsStr(ProgInstance::getUpTime(false))))
					("fromLastCrash",ConstStrA(getTimeAsStr(ProgInstance::getUpTime(true)))));

	return out;*/
}

StatHandler::~StatHandler() {
	stopDumpJob();
}

void StatHandler::scheduleDump( StatHandler::DumpArgs args) {
	time_t curTm = TimeStamp::now().asUnix();
	time_t nextTm = ((curTm + 300) / 300) * 300; //every 5 minutes
	time_t remain = nextTm - curTm;
	args.lastclk = ProgInstance::getCPUTime();
	args.lasttm = curTm;
	dumpJob = this->scheduler->schedule(
			Action::create(this, &StatHandler::dumpNow, args), (natural)remain);
}

void StatHandler::dumpStatsToFile(FilePath fname, HttpServer *server) {
	if (dumpJob) {
		this->scheduler->cancel(dumpJob,false);
	}
	this->scheduler = &server->getIfc<IJobScheduler>();
	scheduleDump(DumpArgs(server,fname));
}

void StatHandler::stopDumpJob() {
	if (dumpJob) {
		void *s;
		do {
			s= dumpJob;
			scheduler->cancel(s,false);
		}while (s != dumpJob);
		dumpJob = 0;
	}
}

StatHandler::StatHandler():dumpJob(0),scheduler(0) {
}

void StatHandler::dumpNow(DumpArgs args) {
	TimeStamp d = TimeStamp::now();
	bool header = !IFileIOServices::getIOServices().canOpenFile(args.dumpPath,IFileIOServices::fileOpenWrite);
	SeqFileOutBuff<> fout(args.dumpPath,OpenFlags::append|OpenFlags::create|OpenFlags::createFolder);
	PrintTextA print(fout);
	if (header) {
		print("date,requests_avg,requests_min,requests_max,"
				"threads_avg,threads_min,threads_max,"
				"threadsIdle_avg,threadsIdle_min,threadsIdle_max,"
				"connections_avg,connections_min,connections_max,"
				"latency_avg,latency_min,latency_max,"
				"worktime_avg,worktime_min,worktime_max,"
				"cpu,memory,crashCount,uptime\n");
	}

	const HttpStats &st = args.server->getStats();
	JSON::PFactory f = JSON::createFast();

	print("%1,") << d.getFloat()+25569;
	print("%1,%2,%3,") << avgField(st.requests,f,300)->getStringUtf8()
						<< minField(st.requests,f,300)->getStringUtf8()
						<< maxField(st.requests,f,300)->getStringUtf8();
	print("%1,%2,%3,") << avgField(st.threads,f,300)->getStringUtf8()
						<< minField(st.threads,f,300)->getStringUtf8()
						<< maxField(st.threads,f,300)->getStringUtf8();
	print("%1,%2,%3,") << avgField(st.idleThreads,f,300)->getStringUtf8()
						<< minField(st.idleThreads,f,300)->getStringUtf8()
						<< maxField(st.idleThreads,f,300)->getStringUtf8();
	print("%1,%2,%3,") << avgField(st.connections,f,300)->getStringUtf8()
						<< minField(st.connections,f,300)->getStringUtf8()
						<< maxField(st.connections,f,300)->getStringUtf8();
	print("%1,%2,%3,") << avgField(st.latency,f,300)->getStringUtf8()
						<< minField(st.latency,f,300)->getStringUtf8()
						<< maxField(st.latency,f,300)->getStringUtf8();
	print("%1,%2,%3,") << avgField(st.worktime,f,300)->getStringUtf8()
						<< minField(st.worktime,f,300)->getStringUtf8()
						<< maxField(st.worktime,f,300)->getStringUtf8();

	natural cputm = ProgInstance::getCPUTime();
	time_t curTm = d.asUnix();
	if (curTm == args.lasttm) print(",");
	else print("%1,") << (double)(cputm - args.lastclk)/(double)(curTm - args.lasttm)/10.0;
	print("%1,") << ProgInstance::getMemoryUsage();
	print("%1,") << ProgInstance::getRestartCounts();
	print("%1\n") << (double)ProgInstance::getUpTime(true)/(double)TimeStamp::daySecs;
	scheduleDump(args);
}

}
