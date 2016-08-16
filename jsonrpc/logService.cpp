/*
 * logService.cpp
 *
 *  Created on: Aug 16, 2016
 *      Author: ondra
 */

#include "logService.h"

#include <lightspeed/base/text/textOut.h>
#include <lightspeed/utils/json/jsonserializer.h>

#include "lightspeed/base/debug/LogProvider.h"
namespace jsonrpc {


LogService::LogService()
	:logRotateCounter(0)
{


}

void LogService::logMethod(BredyHttpSrv::IHttpRequestInfo& invoker, ConstStrA methodName,
		const JSON::ConstValue& params, const JSON::ConstValue& context,
		const JSON::ConstValue& logOutput) {

	BredyHttpSrv::IHttpPeerInfo &pinfo = invoker.getIfc<BredyHttpSrv::IHttpPeerInfo>();
	ConstStrA peerAddr = pinfo.getPeerRealAddr();
	logMethod(peerAddr,methodName,params,context,logOutput);

}


void LogService::logMethod(ConstStrA source, ConstStrA methodName,
		const JSON::ConstValue& params, const JSON::ConstValue& context,
		const JSON::ConstValue& logOutput) {

	if (logfile == nil && lvlog == nil) return;

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
	JSON::Serializer<Stream::InvokeClassType> ser(stream,false);
	ser.serializeString(source);
	stream.write(',');
	ser.serializeString(methodName);
	stream.write(',');
	if (params != null) ser.serialize(params);else ser.serialize(JSON::getConstant(JSON::constNull));
	stream.write(',');
	if (context!= null) ser.serialize(context);else ser.serialize(JSON::getConstant(JSON::constNull));
	stream.write(',');
	if (logOutput!= null) ser.serialize(logOutput);else ser.serialize(JSON::getConstant(JSON::constNull));
	stream.write('[');
	print("]\n");


}

void LogService::setFile(String fname) {
}

void LogService::setLogOutput(ILogOutput* lvlog, ILogOutput::LogType type) {
}


void LogService::openLog() {
}

} /* namespace jsonrpc */
