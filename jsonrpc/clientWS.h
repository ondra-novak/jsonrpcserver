/*
 * clientWS.h
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_JSONRPC_CLIENTWS_H_
#define LIBS_JSONRPCSERVER_JSONRPC_CLIENTWS_H_

#include <lightspeed/base/containers/queue.h>
#include "iclient.h"
#include "lightspeed/base/actions/promise.h"

#include "lightspeed/mt/thread.h"

#include "lightspeed/base/containers/map.h"
#include "../httpclient/webSocketsClient.h"
namespace jsonrpc {
/// WSRPC object
/** There is mayor difference between standard HTTP/S and WS interface. HTTP client
 * doesn't require any background service, it can send request and wait for reply without
 * need to do anything on background. In contrast, ClientWS supports notifications and callback
 * functions that can be called asychronously. The client will need some background processing.
 * It require at least running thread or registration on NetworkEventListener
 *
 * */
class ClientWS: public IClient, public BredyHttpClient::WebSocketsClient {
public:

	typedef BredyHttpClient::WebSocketsClient Super;
	///creates websocket jsonrpc client.
	/**
	 *
	 * @param cfg
	 */
	ClientWS(const ClientConfig &cfg);

	///Connects the websockets interface
	/**
	 * @param listener pointer to event listener to register to receive notifications
	 *
	 * @note event listener must not be destroyed before this object. or before disconnect() is issued
	 *
	 * You can anytime to switch to different listener.
	 *
	 * If connection is closed by remote site, function onLostConnection is called. Object doesn't
	 * perform automatic reconnect. You have to implement it in the handler. Note that you should
	 * avoid to call connect() diectly. It is recomended to schedule the action.
	 *
	 * @note If exception is thrown because connection failed, you can call reconnect() to repeate
	 * the attempt.
	 */
	void connect(PNetworkEventListener listener);


	///Perform RPC call asynchronously
	/**
	 *
	 * @param jsonFactory reference to JSON factory which will be used to parse response and create JSON result. Note that
	 *  factory object must be MT save, or you should avoid to access it until the result is received
	 * @param method name of the method
	 * @param params parameters, must be either array or object
	 * @param context object which contains
	 * @return function returns future which can be resolved later
	 *
	 *
	 */
	Future<Result> callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

	Result call(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

protected:


	///called when notify received
	virtual void onNotify(ConstStrA notifyMethod, JSON::ConstValue params) {
		(void)notifyMethod;
		(void)params;
	}

	///Called when there is error while receiving from websockets.
	/**
	 *
	 * @param msg contains text message received from the connection.
	 * @param exception is set to true, if called i n exception handler
	 *
	 * default implementation closes connection
	 */

	virtual void onReceiveError(ConstStrA msg, bool exception) {
		(void)msg;(void)exception;
	}

	virtual JSON::ConstValue onBackwardRPC(ConstStrA msg, const JSON::ConstValue params);

	///Called when connection has been established
	/**
	 * @note Internal lock is held in this function. You should avoid to block the thread. If you
	 * need to send request in this thread, you have to use callAsync, because call() will deadlock.
	 * You also should avoid to wait for future.
	 */
	virtual void onConnect();



	JSON::PFactory jsonFactory;
	StringA cfgurl;


	Map<natural, Promise<Result> > waitingResults;
	natural idcounter;




	void onTextMessage(ConstStrA msg);

	void sendResponse(JSON::ConstValue id, JSON::ConstValue result, JSON::ConstValue error);

	virtual void onLostConnection(natural);


};
} /* namespace snapytap */

#endif /* CLIENTWS_H_ */
