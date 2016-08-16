/*
 * logService.h
 *
 *  Created on: Aug 16, 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_932eijwo39r_LOGSERVICE_H_
#define JSONRPCSERVER_JSONRPC_932eijwo39r_LOGSERVICE_H_
#include <lightspeed/base/debug/dbglog_internals.h>

#include "idispatch.h"

namespace jsonrpc {

class LogService: public ILog {
public:
	LogService();

	virtual void logMethod(BredyHttpSrv::IHttpRequestInfo &invoker, ConstStrA methodName, const JSON::ConstValue &params, const JSON::ConstValue &context, const JSON::ConstValue &logOutput);
	virtual void logMethod(ConstStrA source, ConstStrA methodName, const JSON::ConstValue &params, const JSON::ConstValue &context, const JSON::ConstValue &logOutput) ;

	///Sets output file
	void setFile(String fname);
	///Redirects output to the standard log output
	/**
	 * @param lvlog pointer to ILogOutput instance
	 * @param type records from RPC trafic are logged under same log level. Specify the log level here
	 *
	 * @note - LogService sends formatted output directly to the log instance, bypassing any formatting. If you
	 *  mix it with standard log output, you should adjust the format;
	 */
	void setLogOutput(ILogOutput *lvlog, ILogOutput::LogType type);

protected:

	void openLog();

	String logFileName;
	POutputStream logfile;
	volatile atomic logRotateCounter;
	mutable FastLock lock;
	Pointer<ILogOutput> lvlog;
	void logRotate();


};

} /* namespace jsonrpc */

#endif /* JSONRPCSERVER_JSONRPC_932eijwo39r_LOGSERVICE_H_ */
