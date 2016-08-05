/*
 * ServiceContext.cpp
 *
 *  Created on: Sep 28, 2012
 *      Author: ondra
 */

#include "ServiceContext.h"
#include <lightspeed/base/containers/autoArray.tcc>
#include "lightspeed/base/debug/dbglog.h"

namespace jsonsrv {

	using namespace LightSpeed;
	using namespace LightMySQL;


ServiceContext::ServiceContext( MySQLConfig connParams):connParams(connParams),logmaster(true),logslave(false)	{

	okResult = JSON::create()->newValue(true);
	okResult = okResult.getMT();
	if (this->connParams.master.logObject == nil) this->connParams.master.logObject = &logmaster;
	if (this->connParams.slave.logObject == nil) this->connParams.slave.logObject = &logslave;
	pool.init(this->connParams,0);
}

ServiceContext::Connection::Connection(ServiceContext* owner)
	:owner(owner)
{

}

ServiceContext::Connection* ServiceContext::getContext(BredyHttpSrv::IHttpRequest* request) {
	BredyHttpSrv::IHttpHandlerContext* ctx = request->getRequestContext();
	if (ctx != 0) {
		Connection* c = dynamic_cast<Connection*>(ctx);
		if (c != 0 && c->owner == this)
			return c;
	}
	Connection* c = new Connection(this);
	request->setRequestContext(c);
	return c;
}

ServiceContext::Connection *ServiceContext::createContext() {
	return new(allocator) Connection(this);
}

ServiceContext::Connection* ServiceContext::getContext( jsonsrv::RpcRequest* r) {
	//NOTE - can throw exception
	IHttpRequest &rq = r->httpRequest->getIfc<IHttpRequest>();
	return getContext(&rq);
}


LightMySQL::Transaction ServiceContext::Connection::getTransactRO() {
	//use master when an master is already opened for this request
	//this covers situations when read immediately follows write - operation expects, that it receives fresh values back
	//it is still better to keep all in the same transaction.
	if (master != nil) {
		MySQLResPtr &ptr = master;
		return ptr->getTransact();
	} else {
		if (slave == nil) slave = MySQLResPtr(owner->pool.getSlave());
		MySQLResPtr &ptr = slave;
		return ptr->getTransact();
	}
}

LightMySQL::Transaction ServiceContext::Connection::getTransactRW() {
	if (master == nil) master = MySQLResPtr(owner->pool.getMaster());
	MySQLResPtr &ptr = master;
	return ptr->getTransact();
}

MySQLResPtr ServiceContext::Connection::getMaster() {
	return master;
}

MySQLResPtr ServiceContext::Connection::getSlave() {
	return slave;
}

}

/* namespace snapytap */

