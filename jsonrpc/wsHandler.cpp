/*
 * wsHandler.cpp
 *
 *  Created on: 3. 10. 2016
 *      Author: ondra
 */

#include "wsHandler.h"

#include "iclient.h"
#include "rpcnotify.h"
namespace jsonrpc {


WSHandler::WSHandler(IDispatcher& dispatcher)
	:dispatcher(dispatcher),events(0)
{

}

WSHandler::WSHandler(IDispatcher& dispatcher,const JSON::Builder& json)
	:dispatcher(dispatcher),events(0), json(json)
{
}

void WSHandler::setListener(IWSHandlerEvents* handler) {
	events = handler;
}

WeakRef<IWSHandlerEvents> WSHandler::getListener() const {
	return events;
}


class WSConnection: public BredyHttpSrv::WebSocketConnection, public IRpcNotify, public IClient, public BredyHttpSrv::IHttpContextControl {
public:
	WSConnection();
	virtual void onConnect();
	virtual void onTextMessage(ConstStrA msg);
	virtual void onBinaryMessage(ConstBin msg);
	virtual void onCloseOutput(natural code);
	virtual void onPong(ConstBin msg);

	virtual PreparedNotify prepare(LightSpeed::ConstStrA name, LightSpeed::JSON::ConstValue arguments);
	virtual void sendPrepared(const PreparedNotify &prepared, TimeoutControl tmControl = shortTimeout);
	virtual void sendNotification(LightSpeed::ConstStrA name, LightSpeed::JSON::ConstValue arguments, TimeoutControl tmControl = standardTimeout);
	virtual void setContext(BredyHttpSrv::IHttpHandlerContext *context);
	virtual BredyHttpSrv::IHttpHandlerContext *getContext() const;
	virtual void dropConnection();
	virtual void closeConnection(natural code);
	virtual Future<void> onClose();

	virtual Future<Result> callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

	virtual ConstStrA getMethod() const;
	virtual ConstStrA getPath() const;
	virtual ConstStrA getProtocol() const;
	virtual HeaderValue getHeaderField(ConstStrA field) const;
	virtual HeaderValue getHeaderField(HeaderField field) const;
	virtual bool enumHeader(HdrEnumFn fn) const;
	virtual ConstStrA getBaseUrl() const;
	virtual StringA getAbsoluteUrl() const;
	virtual StringA getAbsoluteUrl(ConstStrA relpath) const;
	virtual bool keepAlive() const;
	virtual void beginIO();
	virtual void endIO();
	virtual void setRequestContext(IHttpHandlerContext *context);
	virtual void setConnectionContext(IHttpHandlerContext *context);
	virtual IHttpHandlerContext *getRequestContext() const;
	virtual IHttpHandlerContext *getConnectionContext() const;

};



BredyHttpSrv::WebSocketConnection* WSHandler::onNewConnection(
		IRuntimeAlloc& alloc, BredyHttpSrv::IHttpRequest& request, ConstStrA vpath) {

	return new WSConnection;

}


} /* namespace jsonrpc */
