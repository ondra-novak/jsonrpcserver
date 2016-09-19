/*
 * ServiceContext.h
 *
 *  Created on: Sep 28, 2012
 *      Author: ondra
 */

#ifndef SNAPYTAP_SERVICECONTEXT_H_
#define SNAPYTAP_SERVICECONTEXT_H_
#include "lightmysql/connection.h"
#include "lightmysql/transaction.h"
#include "lightspeed/base/containers/optional.h"
#include "lightspeed/base/memory/poolalloc.h"
#include "lightmysql/resourcepool.h"
#include "lightmysql/mydbglog.h"
#include "../jsonrpc/rpchandler.h"


namespace jsonsrv {

class ServiceContext {
public:
	ServiceContext(LightMySQL::MySQLConfig cfg);


	class Connection: public BredyHttpSrv::IHttpHandlerContext {
	public:
		Connection(ServiceContext *owner);

		ServiceContext *owner;

		ServiceContext* getOwner() const {return owner;}
		LightMySQL::Transaction getTransactRO();
		LightMySQL::Transaction getTransactRW();
		LightMySQL::MySQLResPtr getMaster();
		LightMySQL::MySQLResPtr getSlave();
	protected:

		Optional<LightMySQL::MySQLResPtr> master, slave;



	};

	Connection *getContext( BredyHttpSrv::IHttpContextControl *request);
	Connection *getContext( jsonsrv::RpcRequest *r);
	Connection *createContext();

	const LightMySQL::MySQLConfig & getConnParams() const {
		return connParams;
	}

	JSON::PNode okResult;

	LightMySQL::MySQLConfig connParams;
	LightMySQL::MySQLReport logmaster,logslave;
	LightMySQL::MySQLMasterSlavePool pool;



protected:
	PoolAlloc allocator;
};

typedef ServiceContext Database;

} /* namespace snapytap */
#endif /* SNAPYTAP_SERVICECONTEXT_H_ */
