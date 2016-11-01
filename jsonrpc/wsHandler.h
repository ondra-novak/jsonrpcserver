/*
 * wsHandler.h
 *
 *  Created on: 3. 10. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONSERVER_WSHANDLER_H_
#define JSONRPC_JSONSERVER_WSHANDLER_H_
#include "../httpserver/webSockets.h"
#include "lightspeed/base/memory/weakref.h"
#include "lightspeed/utils/json/jsonbuilder.h"


namespace jsonrpc {

class IDispatcher;
class IPeer;

using namespace LightSpeed;

struct WSContext {
	IPeer &peer;
	IDispatcher &dispatcher;

	WSContext(IPeer &peer,IDispatcher &dispatcher)
		:peer(peer),dispatcher(dispatcher) {}
};



class IWSHandlerEvents {
public:
	virtual void onOpen(const WSContext &request) = 0;
	virtual void onClose(const WSContext &request) = 0;
};

class WSHandler: public BredyHttpSrv::AbstractWebSocketsHandler {
public:



	WSHandler(IDispatcher &dispatcher);

	void setListener(IWSHandlerEvents *handler);
	WeakRef<IWSHandlerEvents> getListener() const;
protected:

	virtual BredyHttpSrv::WebSocketConnection * onNewConnection(IRuntimeAlloc &alloc, BredyHttpSrv::IHttpRequest &request, ConstStrA vpath);

	IDispatcher &dispatcher;
	WeakRefTarget<IWSHandlerEvents> events;
};

} /* namespace jsonrpc */

#endif /* JSONRPC_JSONSERVER_WSHANDLER_H_ */
