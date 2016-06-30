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
#include "../httpserver/abstractWebSockets.h"
#include "lightspeed/base/actions/promise.h"

#include "lightspeed/mt/thread.h"

#include "lightspeed/base/containers/map.h"
namespace jsonrpc {
/// WSRPC object
/** There is mayor difference between standard HTTP/S and WS interface. HTTP client
 * doesn't require any background service, it can send request and wait for reply without
 * need to do anything on background. In contrast, ClientWS supports notifications and callback
 * functions that can be called asychronously. The client will need some background processing.
 * It require at least running thread or registration on NetworkEventListener
 *
 * */
class ClientWS: public IClient {
public:

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
	 * perform automatic reconnect. You have to implement it in the handler. Note that you should
	 * avoid to call connect() diectly. It is recomended to schedule the action.
	 *
	 * @note If exception is thrown because connection failed, you can call reconnect() to repeate
	 * the attempt.
	 */
	void connect(PNetworkEventListener listener);

	///Disconnects websocket
	/** closes connection and disables reconnect() feature */
	void disconnect();

	///Reconnects the disconnected websocket client
	/** Works only if called after onLostConnection() is issued.
	 *
	 * @retval true reconnected or client has been already connected
	 * @retval false disconnected, cannot be reconnected
	 */
	bool reconnect();

	///determines whether WS client is disconnected
	/**
	 * @retval true client is disconnected (reconnect will fail)
	 * @retval false client is connected or in reconnecting state. reconnect might success
	 */
	bool isDisconnected() const;

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


	///called when notify received
	virtual void onNotify(ConstStrA notifyMethod, JSON::ConstValue params);
	///called when stream is connected
	virtual void onConnect();

	///called when connection lost
	/**
	 * You can use this callback to create autoreconnect function
	 *
	 * Because function is called in context of network-listener's thread, you should
	 * not call reconnect() directly. Instead you should create a thread which will reconnect after
	 * a short delay. The delay should raise with unsuccesful requests to reconnect
	 *
	 *
	 * @param code reason for reconnect. If naturalNull - no reason given
	 */
	virtual void onLostConnection(natural code);


	///Called when there is error while receiving from websockets.
	/**
	 *
	 * @param msg contains text message received from the connection.
	 * @param exception is set to true, if called i n exception handler
	 *
	 * default implementation closes connection
	 */

	virtual void onReceiveError(ConstStrA msg, bool exception);

	virtual JSON::ConstValue onBackwardRPC(ConstStrA msg, const JSON::ConstValue params);

protected:


	class WsConn;
	friend class BredyHttpSrv::AbstractWebSocketConnection<WsConn, false>;
	class WsConn: public BredyHttpSrv::AbstractWebSocketConnection<WsConn, false>, public RefCntObj, public ISleepingObject {
	public:
		WsConn(ClientWS &owner, PInOutStream data):owner(owner),data(data) {}


		natural stream_read(byte *buffer, natural length);
		void stream_write(const byte *buffer, natural length);
		void stream_closeOutput();

		void onTextMessage(ConstStrA msg);
		void onCloseOutput(natural code);
		void onPong(ConstBin msg);
		void onBinaryMessage(ConstBin msg);

		virtual void wakeUp(natural reason = 0) throw();


	protected:
		ClientWS &owner;
		PInOutStream data;
	};

	typedef RefCntPtr<WsConn> PWsConn;


	///Called before connection is initialized
	/** implementation can append own headers to http connection
	 * Then implementation must call original implementation to finish ws hanshake.
	 * After the return implementation can retrieve status code and headers
	 *
	 * @param http http connection
	 * @return connected stream
	 * @exception HttpStatusException if other status then 200 is returned, function throws HttpStatusException
	 */
	virtual PNetworkStream onInitConnection(BredyHttpClient::HttpClient &http);

	PNetworkStream stream;
	PWsConn conn;
	ClientConfig cfg;
	PNetworkEventListener listener;
	Thread failWait;

	FastLockR controlLock;

	struct Request {
		natural id;
		StringA request;
		Promise<Result> result;

		Request (natural id,StringA request,Promise<Result> result);
	};

	Map<natural, Promise<Result> > waitingResults;
	Queue<Request> waitingRequests;
	FastLockR callLock;
	natural idcounter;



	void onTextMessage(ConstStrA msg);
	void onCloseOutput(natural code);

	void sendResponse(JSON::ConstValue id, JSON::ConstValue result, JSON::ConstValue error);
	void rearmStream();

private:
	void disconnectInternal();
	void connectInternal();
	void callOnLostConnectionOnExit(natural code);
};
} /* namespace snapytap */

#endif /* CLIENTWS_H_ */
