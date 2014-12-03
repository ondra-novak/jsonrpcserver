/*
 * clientAdapter.h
 *
 *  Created on: Nov 20, 2012
 *      Author: ondra
 */

#ifndef JSONRPC_CLIENTADAPTER_H_
#define JSONRPC_CLIENTADAPTER_H_

#include "iclient.h"
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/containers/linkedList.h"


namespace jsonrpc {

class ClientAdapter: public IClient {
public:
	ClientAdapter();
	ClientAdapter(IRuntimeAlloc &alloc);

	virtual JSON::PNode getResult(Ticket ticket, bool *setError) ;
	virtual bool isTicketValid(Ticket ticket) const ;
	virtual void clearAllResults() ;
	virtual void clearResult(Ticket ticket) ;
	virtual bool hasNotify() const ;
	virtual JSON::PNode getNotify(bool *setError) ;
	virtual void setContext(ConstStrA contextName, JSON::PNode context) ;
	virtual JSON::PNode getContext(ConstStrA contextName) const;
	virtual bool enumContext(const JSON::IEntryEnum &enumCb) ;
	virtual void clearContext();

	virtual JSON::IFactory *getFactory();
protected:

	struct RPCResult {
		JSON::PNode data;
		bool isError;
	};
	typedef LightSpeed::Map<Ticket, RPCResult> Results;
	typedef LightSpeed::LinkedList<RPCResult> Notifications;

	Results results;
	Notifications notifications;
	JSON::PFactory factory;
	JSON::PNode context;
	Ticket nextTicketId;

	JSON::PNode extractResult(const RPCResult &res, bool *setError);
	Ticket allocTicket() {return nextTicketId++;}
};

} /* namespace jsonrpc */
#endif /* JSONRPC_
CLIENTADAPTER_H_ */
