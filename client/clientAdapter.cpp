/*
 * clientAdapter.cpp
 *
 *  Created on: Nov 20, 2012
 *      Author: ondra
 */

#include "clientAdapter.h"

namespace jsonrpc {

ClientAdapter::ClientAdapter():factory(JSON::create()),nextTicketId(1) {}

ClientAdapter::ClientAdapter(IRuntimeAlloc& alloc):factory(JSON::create(alloc)),nextTicketId(1) {}

JSON::PNode ClientAdapter::getResult(Ticket ticket, bool* setError) {
	bool found;
	Results::Iterator iter = results.seek(ticket, &found);
	if (!found) throw InvalidClientTicketException(THISLOCATION,ticket);
	RPCResult res = iter.peek().value;
	results.erase(iter);
	return extractResult(res,setError);
}


bool ClientAdapter::isTicketValid(Ticket ticket) const {
	return results.find(ticket) != 0;
}

void ClientAdapter::clearAllResults() {
	results.clear();
	notifications.clear();
}

void ClientAdapter::clearResult(Ticket ticket) {
	results.erase(ticket);
}

bool ClientAdapter::hasNotify() const {
	return !notifications.empty();
}

JSON::PNode ClientAdapter::getNotify(bool* setError) {
	RPCResult res = notifications.getFirst();
	notifications.eraseFirst();
	return extractResult(res,setError);

}

void ClientAdapter::setContext(ConstStrA contextName, JSON::PNode context) {
	if (context == nil) {
		context = factory->object();
	}
	context->erase(contextName);
	context->add(contextName,context);
}

JSON::PNode ClientAdapter::getContext(ConstStrA contextName) const {
	if (context == nil) return nil;
	return context->getVariable(contextName);
}

bool ClientAdapter::enumContext(const JSON::IEntryEnum& enumCb) {
	if (context == nil) return true;
	return context->enumEntries(enumCb);
}
 /* namespace jsonrpc */
const char *exception_InvalidClientTicket = "Invalid client ticket";

void ClientAdapter::clearContext() {
	context = nil;
}


JSON::IFactory* ClientAdapter::getFactory() {
	return factory;
}


JSON::PNode ClientAdapter::extractResult(const RPCResult& res, bool* setError) {
	if (setError == 0) {
		if (res.isError) throw RpcError(THISLOCATION, res.data);
		else return res.data;
	} else {
		*setError = res.isError;
		return res.data;
	}
}

	}

