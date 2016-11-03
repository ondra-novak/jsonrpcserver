/*
 * logService.cpp
 *
 *  Created on: Aug 16, 2016
 *      Author: ondra
 */

#include "logService.h"

#include <lightspeed/base/streams/standardIO.h>
#include <lightspeed/base/text/textOut.tcc>
#include <lightspeed/utils/json/jsonserializer.tcc>

#include "lightspeed/base/debug/LogProvider.h"

namespace jsonrpc {


LogService::LogService()
	:logRotateCounter(0)
{


}

void LogService::logMethod(BredyHttpSrv::IHttpRequestInfo& invoker, StrView methodName,
		const JValue& params, const JValue& context,
		const JValue& logOutput) {

	BredyHttpSrv::IHttpPeerInfo &pinfo = invoker.getIfc<BredyHttpSrv::IHttpPeerInfo>();
	ConstStrA peerAddr = pinfo.getPeerRealAddr();
	logMethod(peerAddr,methodName,params,context,logOutput);

}


void LogService::logMethod(StrView source, StrView methodName,
		const JValue& params, const JValue& context,
		const JValue& logOutput) {

	POutputStream lgf = logfile;

	if (lgf == nil && lvlog == nil) return;

	if (logRotateCounter != DbgLog::rotateCounter) {
		if (DbgLog::needRotateLogs(logRotateCounter)) {
			logRotate();
		}
	}

	typedef AutoArrayStream<char, SmallAlloc<4096> > Stream;
	typedef TextOut<Stream &, SmallAlloc<32> > Print;
	Stream stream;
	Print print(stream);

	AbstractLogProvider::LogTimestamp tms;


	print("%{04}1/%{02}2/%{02}3 %{02}4:%{02}5:%{02}6 - ")
		<< tms.year << tms.month << tms.day
		<< tms.hour << tms.min << tms.sec;

	print("[");
	auto wrfn=[&](char c){stream.write(c);};
	typedef decltype(wrfn) WrFn;

	class MySer: public json::Serializer<WrFn> {
	public:
		using json::Serializer<WrFn>::Serializer;
		using json::Serializer<WrFn>::writeString;
		void writeString(ConstStrA x) {
			writeString(json::StringView<char>(x.data(),x.length()));
		}
		void writeUndef() {
			write("null");
		}

	};
	MySer ser(wrfn);
	ser.writeString(source);
	stream.write(',');
	ser.writeString(methodName);
	stream.write(',');
	if (params.defined()) ser.serialize(params);else ser.writeUndef();
	stream.write(',');
	if (context.defined()) ser.serialize(context);else ser.writeUndef();
	stream.write(',');
	if (logOutput.defined()) ser.serialize(logOutput);else ser.writeUndef();
	print("]\n");

	if (lgf != null) {
		Synchronized<FastLock> _(lock);
		lgf->writeAll(stream.data(),stream.length());
		lgf->flush();
	}
	if (lvlog != null) {
		lvlog->logLine(stream.getArray(), ILogOutput::logProgress,1);
	}

}

void LogService::setFile(String fname) {
	logFileName = fname;
	logfile = null;
	openLog();
}

void LogService::setLogOutput(AbstractLogProvider* lvlog) {
	this->lvlog = lvlog;
}

void LogService::openLog() {
	if (logFileName.empty()) return;
	Synchronized<FastLock> _(lock);
	try {
		IFileIOServices &svc = IFileIOServices::getIOServices();
		logfile = svc.openSeqFile(logFileName,IFileIOServices::fileOpenWrite,OpenFlags::append|OpenFlags::create|OpenFlags::shareDelete|OpenFlags::shareRead|OpenFlags::shareWrite).get();
		logfile->enableMTAccess();
	} catch (...) {

	}
}

void LogService::logRotate() {
	openLog();
}

} /* namespace jsonrpc */
