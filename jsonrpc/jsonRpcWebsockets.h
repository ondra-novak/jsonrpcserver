/*
 * jsonRpcWebsockets.h
 *
 *  Created on: 9.6.2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_JSONRPCWEBSOCKETS_H_
#define JSONRPCSERVER_JSONRPC_JSONRPCWEBSOCKETS_H_
#include "../httpserver/webSockets.h"
#include "ijsonrpc.h"
#include "lightspeed/base/actions/promise.h"
#include "lightspeed/base/containers/map.h"

#include "iclient.h"
#include "rpcnotify.h"



namespace jsonsrv {


using namespace BredyHttpSrv;

class JsonRpcWebsocketsConnection;


class JsonRpcWebsocketsConnection: public WebSocketConnection, public IRpcNotify, public jsonrpc::IClient{
public:
	JsonRpcWebsocketsConnection(IHttpRequest &request, IJsonRpc &handler, StringA openMethod);

	///Sends notification to the client
	/**
	 * Server can anytime send notification to the client.
	 *
	 * @param name name of notification
	 * @param arguments arguments
	 *
	 * @note notification is send with id=null as specification says.
	 */
	void sendNotification(ConstStrA name, JSON::ConstValue arguments, TimeoutControl tmControl);

	PreparedNotify prepare(LightSpeed::ConstStrA name, LightSpeed::JSON::ConstValue arguments);

	void sendPrepared(const PreparedNotify &ntf, TimeoutControl tmControl);



	///Server call method on the client
	/**
	 * Calls method, but operation is asynchronous. Server cannot block thread and wait for
	 * client's respone, because it can block processing of other messages
	 *
	 * @param name name of method
	 * @param arguments arguments
	 * @return Promise which is filled once response arrives. If response is success,
	 * result is put to the promise as resolution. If response is failure, error
	 * is thrown through the promise as RpcError
	 */
	virtual Future<Result> callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

	///Finds connection from the http request
	/** request reference can be picked up at RpcRequest
	 *
	 * You will need pointer to connection to send messages asynchronously.
	 *
	 * To safety detect connection termination, use addCloseEventListener to
	 * listen that situation
	 *
	 *
	 *  */
	static JsonRpcWebsocketsConnection *getConnection(IHttpRequestInfo &request);

	///Sets context of this connection
	/**
	 * @param context new context variable. Function takes ownership of the object and
	 * automatically destroys the object once connection is terminated. This can be used
	 * in benefit to notify components about closed connection. During process of closing
	 * connection, destructor of the handler-context is called
	 */
	void setContext(IHttpHandlerContext *context);

	///Retrieves pointer to current context
	/**
	 * @return pointer to current context or NULL, if context is not set.
	 */
	IHttpHandlerContext *getContext() const;

	virtual void closeConnection(natural code=1000) {WebSocketConnection::closeConnection(code);}

	virtual void dropConnection();

	~JsonRpcWebsocketsConnection();


	virtual Future<void> onClose();


protected:
	virtual void onTextMessage(ConstStrA msg);
	virtual void onCloseOutput(natural);
	virtual void onConnect();

	IJsonRpc &handler;
	IJsonRpcLogObject *logobject;
	natural nextPromiseId;
	typedef Map<natural, Promise<Result> > WaitingPromises;

	WaitingPromises waitingPromises;

	FastLockR lock;
	JSON::Builder json;
	IHttpRequest &http;

	Future<void> onCloseFuture;

	StringA openMethod;
	AllocPointer<IHttpHandlerContext> context;
	//context reserved for JsonRpc through setConnectionContext -
	//it is stored separatedly to prevent interfering with websocket's context
	AllocPointer<IHttpHandlerContext> jsonrpc_context;

	class HttpRequestWrapper;

	Optional<PreparedNotify> prepared;



private:
	//do not call sendTextMessage directly
	//it can break protocol
	using WebSocketConnection::sendTextMessage;

};


class JsonRpcWebsockets: public AbstractWebSocketsHandler {
public:
	///Initialize websocket interface
	/**
	 * @param handler reference to object that process JsonRPC requests
	 * @param openMethod name of method called right after connection is established.
	 *
	 * OpenMethod is called without arguments and context and can be detected as notification. Return
	 * value of the call is ignored. If exception is thrown, connection is closed
	 */
	JsonRpcWebsockets(IJsonRpc &handler,StringA openMethod = StringA());

protected:
	virtual WebSocketConnection * onNewConnection(IRuntimeAlloc &alloc, IHttpRequest &request, ConstStrA vpath);

	IJsonRpc &handler;
	StringA openMethod;
};


} /* namespace jsonsrv */
#endif /* JSONRPCSERVER_JSONRPC_JSONRPCWEBSOCKETS_H_ */
