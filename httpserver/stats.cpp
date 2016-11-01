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
	json::var x = getStatsJSON(&request);
	SeqFileOutput out(&request);
	request.header(IHttpRequest::fldContentType,"application/json");
	request.header(IHttpRequest::fldCacheControl,"no-cache, no-store, must-revalidate");
	request.header(IHttpRequest::fldExpires,"0");
	x.serialize([&](char c){request.write(&c,1);});
	return 0;
}

natural StatHandler::onData(IHttpRequest& ) {
	return 0;
}

namespace {
template<typename T>
json::Value avgField( const StatBuffer<T> &b,  natural cnt )
{
	T c = b.getAvg(cnt);
	if (c.isDefined()) return json::Value(c.getValue());
	else return nullptr;
}

template<typename T>
json::Value minField( const StatBuffer<T> &b, natural cnt )
{
	T c = b.getMin(cnt);
	if (c.isDefined()) return json::Value(c.getValue());
	else return nullptr;
}

template<typename T>
json::Value maxField( const StatBuffer<T> &b, natural cnt )
{
	T c = b.getMax(cnt);
	if (c.isDefined()) return json::Value(c.getValue());
	else return nullptr;
}

template<typename T>
json::Object statFields( const StatBuffer<T> &b)
{
	return json::Object
			("avg006",avgField(b,6))
			("avg030",avgField(b,30))
			("avg060",avgField(b,60))
			("avg300",avgField(b,300))
			("min006",minField(b,6))
			("min030",minField(b,30))
			("min060",minField(b,60))
			("min300",minField(b,300))
			("max006",maxField(b,6))
			("max030",maxField(b,30))
			("max060",maxField(b,60))
			("max300",maxField(b,300));
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

json::StringView<char> convToStr(const ConstStrA &x) {
	return json::StringView<char>(x.data(),x.length());
}

}

json::Value StatHandler::getStatsJSON( IHttpRequestInfo *rq) {

	const HttpServer &server = rq->getIfc<HttpServer>();
	const HttpStats &st = server.getStats();
	json::Object out;
	out("request", statFields(st.requests))
		("threads", statFields(st.threads))
		("threadsIdle", statFields(st.idleThreads))
		("connections", statFields(st.connections))
		("latency", statFields(st.latency))
		("worktime", statFields(st.worktime));

	TimeStamp waketime = TimeStamp::fromUnix(ProgInstance::getUpTime(false));

		out("crashCount",ProgInstance::getRestartCounts());
		out("crashesPerDay", ProgInstance::getRestartCounts()/waketime.getFloat());
		out("upTime",json::Object("total",convToStr(getTimeAsStr(ProgInstance::getUpTime(false))))
					("fromLastCrash",convToStr(getTimeAsStr(ProgInstance::getUpTime(true)))));

	return out;
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

static inline ConstStrA operator ~(const json::Value &a) {
	json::StringView<char> z = a.getString();
	ConstStrA zz(z.data,z.length);
	return zz;

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

	print("%1,") << d.getFloat()+25569;
	print("%1,%2,%3,") <<~ avgField(st.requests,300)
						<<~ minField(st.requests,300)
						<<~ maxField(st.requests,300);
	print("%1,%2,%3,") <<~ avgField(st.threads,300)
						<<~ minField(st.threads,300)
						<<~ maxField(st.threads,300);
	print("%1,%2,%3,") <<~ avgField(st.idleThreads,300)
						<<~ minField(st.idleThreads,300)
						<<~ maxField(st.idleThreads,300);
	print("%1,%2,%3,") <<~ avgField(st.connections,300)
						<<~ minField(st.connections,300)
						<<~ maxField(st.connections,300);
	print("%1,%2,%3,") <<~ avgField(st.latency,300)
						<<~ minField(st.latency,300)
						<<~ maxField(st.latency,300);
	print("%1,%2,%3,") <<~ avgField(st.worktime,300)
						<<~ minField(st.worktime,300)
						<<~ maxField(st.worktime,300);

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


