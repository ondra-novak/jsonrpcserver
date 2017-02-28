/*
 * logService.h
 *
 *  Created on: Aug 16, 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_932eijwo39r_LOGSERVICE_H_
#define JSONRPCSERVER_JSONRPC_932eijwo39r_LOGSERVICE_H_

#include "idispatch.h"
#include "lightspeed/base/debug/LogProvider.h"

#include "ilog.h"
namespace jsonrpc {

class LogService: public ILog {
public:
	LogService();

	virtual void logMethod(BredyHttpSrv::IHttpRequestInfo &invoker, StrViewA methodName, const JValue &params, const JValue &context, const JValue &logOutput);
	virtual void logMethod(StrViewA source, StrViewA methodName, const JValue &params, const JValue &context, const JValue &logOutput) ;

	///Sets output file
	void setFile(String fname);
	///Redirects output to the standard log output
	/**
	 * @param lvlog pointer to Std instance
	 * @param type records from RPC trafic are logged under same log level. Specify the log level here
	 *
	 * @note - LogService sends formatted output directly to the log instance, bypassing any formatting. If you
	 *  mix it with standard log output, you should adjust the format;
	 */
	void setLogOutput(AbstractLogProvider *lvlog);

protected:

	void openLog();

	String logFileName;
	POutputStream logfile;
	volatile atomic logRotateCounter;
	mutable FastLock lock;
	Pointer<AbstractLogProvider> lvlog;
	void logRotate();


};

} /* namespace jsonrpc */

#endif /* JSONRPCSERVER_JSONRPC_932eijwo39r_LOGSERVICE_H_ */
