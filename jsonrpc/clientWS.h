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
#include "config.h"
#include "lightspeed/base/actions/promise.h"

#include "lightspeed/mt/thread.h"

#include "lightspeed/base/containers/map.h"
#include "../httpclient/webSocketsClient.h"
#include "rpcerror.h"

using jsonsrv::RpcError;

namespace BredyHttpSrv {
	class IJobScheduler;
	class IHttpRequestInfo;
	class IHttpMapper;
}

namespace jsonsrv {
	class IJsonRpcLogObject;
	class IJsonRpc;
}

namespace LightSpeed {
	class IExecutor;
}

namespace jsonrpc {

class IRpcNotify;
class PreparedNotify;




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

	///When IHttpRequest explored for the http-method, this string will be returned. You can determine, that connection is in client mode
	static ConstStrA httpMethodStr;


	typedef BredyHttpClient::WebSocketsClient Super;
	///creates websocket jsonrpc client.
	/**
	 *
	 * @param cfg
	 */
	ClientWS(const ClientConfig &cfg);

	~ClientWS();


	///Connects the websockets interface
	/**
	 * @param listener pointer to event listener to register to receive notifications
	 *
	 * @note event listener must not be destroyed before this object. or before disconnect() is issued
	 *
	 * You can anytime to switch to different listener.
	 *
	 * If connection is closed by remote site, function onLostConnection is called. Object doesn't
	 * perform automatic reconnect.
	 *
	 *  @note If exception is thrown because connection failed, you can call reconnect() to repeat
	 * the attempt.
	 */
	void connect(PNetworkEventListener listener);

	///More complex configuration for making connection
	struct ConnectConfig {
		///Pointer to network event listener. It can be set to null if you requere to pump messages manually
		/**
		 * @see BredyHttpClient::WebSocketsClient::wakeUp
		 */
		PNetworkEventListener listener;
		///Pointer to scheduler
		/** Scheduler handles automatical reconnects if necesery. You can set to null, if you require
		 * to handle reconnects manually
		 */
		Pointer<BredyHttpSrv::IJobScheduler> scheduler;
		///Pointer to executor
		/** If defined, messages are processed through the executor. If set to null, all messages
		 *  are processed in single thread
		 */
		Pointer<IExecutor> executor;
		/// Pointer to jsonrpc dispatcher
		/** if set to non-null, every method or notification is passed to the dispatcher. Set to null to prevent this */
		Pointer<jsonsrv::IJsonRpc> dispatcher;
		/// Pointer to jsonrpc's logging object
		/** if set to null,  logging is disabled */
		Pointer<jsonsrv::IJsonRpcLogObject> logObject;

		/// Method called through the dispatcher when connection is established.
		StringA connectMethod;

		///Constructs config from the IHttpMapper, which is available during server initialization
		/**
		 * @param mapper reference to a httpmapper, which is available during method ServerMain::onStartServer.
		 * @return returns structure where listener, scheduler and executor is initialized. Dispatcher and logObject
		 * is set to NULL. Initialization causes that WS client will use server's netrowk event listener, server's
		 * scheduler and server's thread pool
		 *
		 *
		 */
		static ConnectConfig fromHttpMapper(BredyHttpSrv::IHttpMapper &mapper);
		///Constructs config from the IHttpRequest, which is available during the http request
		/**
		 * @param request reference to a http request.
		 * @return returns structure where listener, scheduler and executor is initialized. Dispatcher and logObject
		 * is set to NULL.Initialization causes that WS client will use server's netrowk event listener, server's
		 * scheduler and server's thread pool
		 */
		static ConnectConfig fromRequest(BredyHttpSrv::IHttpRequestInfo &request);


	};


	///Connects the websockets interface
	/**
	 *
	 * @param lst pointer to event listener to register to receive notifications
	 * @param scheduler pointer to the scheduler. The scheduler will be used to schedule reconnects when connection
	 * is lost.
	 */
	void connect(const ConnectConfig &config);


	///disconnect the interface
	void disconnect(natural reason=naturalNull);


	///Force reconnect now
	bool reconnect();


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

	///Perform RPC call synchronously
	/**
	 * @param method method name
	 * @param params arguments must be array
	 * @param context context (optional)
	 * @return function returns result (return value + context)
	 *
	 * @note because function blocks current thread, it must not be combined with callAsync (response is
	 * received in response thread, which should not be blocked).
	 */
	Result call(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

	void sendNotify(ConstStrA method, const JSON::ConstValue &params);
	void sendNotify(const PreparedNotify &preparedNotify);
	PreparedNotify prepareNotify(ConstStrA method, const JSON::ConstValue &params);

protected:


	///called when notify received
	/** By default, function calls dispatcher if defined, otherwise notification is ignored
	 *
	 * @param method method which is being notified
	 * @param params arguments of the call
	 * @param context context of the call
	 *
	 * @note notification has no return, there is no response to the caller, because you have no id associated with the call
	 */
	virtual void onNotify(ConstStrA method, const JSON::Value &params, const JSON::Value &context);

	///Called when there is error while receiving from websockets.
	/**
	 *
	 * @param msg contains text message received from the connection.
	 * @param exception is set to true, if called i n exception handler
	 *
	 * default implementation closes connection
	 */

	virtual void onParseError(ConstStrA msg) {
		(void)msg;
	}
	virtual void onDispatchError(JSON::Value v) {
		(void)v;
	}


	///called when income RPC received
	/** By default, function calls the dispatcher if defined, otherwise, RPC exception is returned as the method doesn't exists
	 *
	 * @param method name of the method
	 * @param params arguments of the call
	 * @param context context of the call
	 * @param id request id
	 *
	 * To return value, function must call sendResponse() with apropriate ID. It can be done in different thread
	 */

	virtual void onIncomeRPC(ConstStrA method, const JSON::Value &params, const JSON::Value &context, const JSON::Value &id );

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



	///called with text message received by the web socket
	/** Default implementation parses the message and calls processMessage() */
	virtual void onTextMessage(ConstStrA msg);

	///Processes JSON message, identifies its type and call aproproate handler */
	virtual void processMessage(JSON::Value msg);

	///Formats and sends a response
	/**
	 *
	 * @param id id of the request which response is being send
	 * @param result result
	 * @param error error message
	 *
	 * @note if result is defined, error must be json-null and vice versa
	 */
	virtual void sendResponse(JSON::ConstValue id, JSON::ConstValue result, JSON::ConstValue error);


	virtual void onLostConnection(natural);

	virtual void scheduleReconnect();
	void *reconnectMsg;
	natural reconnectDelay;

	Pointer<BredyHttpSrv::IJobScheduler> scheduler;
	Pointer<jsonsrv::IJsonRpc> dispatcher;
	Pointer<jsonsrv::IJsonRpcLogObject> logObject;
	Pointer<jsonsrv::IExecutor> executor;
	StringA connectMethod;


	//Transfer execution to the executor instead running the messages on the listener's thread
	virtual void wakeUp(natural reason) throw();
	//calls Super::wakeUp, need to called in the executor
	void super_wakeUp(natural reason) throw();

	/*Is locked, when some code is running inside the executor.
	  This prevents disconnection and/or destruction of the object during it and
	  also prevents mistakenly called wakeUp if the request is already processed
	  */
	FastLock inExecutor;
	RefCntPtr<BredyHttpSrv::IHttpRequestInfo> fakeRequest;

	class ClientFakeRequest;

};
} /* namespace snapytap */

#endif /* CLIENTWS_H_ */
