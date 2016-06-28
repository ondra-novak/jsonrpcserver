/*
 * rpcerror.h
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSRV_RPCERROR_H_
#define JSONRPCSRV_RPCERROR_H_
#include <lightspeed/base/exceptions/exception.h>
#include <lightspeed/utils/json/json.h>
#include <lightspeed/base/containers/string.h>


namespace jsonsrv {

using namespace LightSpeed;

struct RpcRequest;



class RpcError: public Exception {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcError(const ProgramLocation &loc, JSON::Value errorNode);
	RpcError(const ProgramLocation &loc, JSON::IFactory *factory, natural status, String statusMessage);
	RpcError(const ProgramLocation &loc,const  RpcRequest *r, natural status, String statusMessage);
	~RpcError() throw() {}
	const JSON::Value &getError() const;

	void message(ExceptionMsg &msg) const;

protected:
	JSON::Value errorNode;
};

class RpcCallError: public Exception {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcCallError(const ProgramLocation &loc, natural status, StringA statusMessage)
		:Exception(loc),status(status),statusMessage(statusMessage) {}

	natural getStatus() const {return status;}
	const StringA &getStatusMessage() const {return statusMessage;}
	~RpcCallError() throw() {}

protected:
	natural status;
	StringA statusMessage;

	void message(ExceptionMsg &msg) const;

};


class RpcMandatoryField: public RpcError {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	RpcMandatoryField(const ProgramLocation &loc, JSON::IFactory *factory, ConstStrA field)
		:RpcError(loc,factory,400,String(ConstStrA("Mandatory field: ")+field)),field(field) {}
	ConstStrA getField() const {return field;}
	~RpcMandatoryField() throw() {}
protected:
	StringA field;


};



}




#endif /* RPCERROR_H_ */
