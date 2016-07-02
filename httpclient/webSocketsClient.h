/*
 * clientWS.h
 *
 *  Created on: 28. 6. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_BREDYHTTPCLIENT_WEBSOCKETSCLIENT_H_654108478979
#define LIBS_JSONRPCSERVER_BREDYHTTPCLIENT_WEBSOCKETSCLIENT_H_654108478979
#include <lightspeed/base/containers/queue.h>

#include "../httpserver/abstractWebSockets.h"
#include "httpClient.h"
#include "lightspeed/mt/fastlock.h"
#include "lightspeed/base/actions/promise.h"
#include "lightspeed/base/containers/queue.h"

namespace BredyHttpClient {
/// WebSockets client
/** WebSocketsClient object is permanently active object in compare to HttpClient which is
 * active only in time of a request. To use the object, you will need one
 * instance of NetworkEventListener at least. This requires extra thread. You
 * can still use the client in single thread application, but this need to
 * check data on connected stream manually and for protocols based on request-response
 * will require special handling between the request and response.
 *
 * To create WebSocketsClient instance, create new class which inherits the WebSocketsClass
 * and overwrite some functions you will need for correct operation.
 * Then construct the instance of this new class  with prepared config ClientConfig.
 * Then you can call connect() which requires an url and the reference to an instance of NetworkEventListener.
 * Once the connection is established, you can sendTextMessage or sendBinMessage and you can receive
 * message through onTextMessage or onBinMessage.
 *
 * Object doesn't support autoreconnect, however it has some support for it. Once
 * the connection is lost, the function onLostConnection is called. Your code can
 * try to call reconnect after some delay. It is strongly recomended to create
 * a thread which will perform the reconnect, or to use a scheduler (which itself
 * is running on separate thread)
 */
class WebSocketsClient: public BredyHttpSrv::AbstractWebSocketConnection<WebSocketsClient, false>,  public ISleepingObject {
public:

	typedef BredyHttpSrv::AbstractWebSocketConnection<WebSocketsClient, false> Super;


	///creates websockets jsonrpc client.
	/**
	 *
	 * @param cfg
	 */
	WebSocketsClient(const ClientConfig &cfg);
	~WebSocketsClient();

	///Connects the websockets interface
	/**
	 * @param url url of server to connect. Must not be empty.
	 * @param listener pointer to event listener to register to receive notifications. This argument
	 *   can be NULL if you don't have listener or if you don't want to install one. In this case
	 *   you should detect whether there are data on the stream and eventually call wakeUp() to
	 *   process them.
	 *	 *
	 *
	 * Object doesn't perform automatic reconnect. However, once the object is in connected
	 * state, you can call the function reconnect() after connection is lost,
	 * without specifying the url and listener.
	 */
	void connect(ConstStrA url, PNetworkEventListener listener);

	///Disconnects websocket
	/** closes connection and disables reconnect() feature
	 *
	 *
	 * @param reason specifies reason to disconnect. Reason is sent to the server. If
	 * reason is equal to naturalNull (default), no reason is sent.
	 *
	 * Recommended reasons are specified in WebSocketsConstants
	  */

	void disconnect(natural reason=naturalNull);

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

	///called when stream is connected
	/** you have to call original implementation for delivery of messages collected during
	 * disconnected pause (waiting for reconnect). It is possible to call this
	 * before initial messages are send or after (for example, if you need to restore
	 * connection state, before the collected messages are sent
	 */
	virtual void onConnect();

	///called when connection lost
	/**
	 * You can use this callback to create auto-reconnect function
	 *
	 * Because function is called in the context of network-listener's thread, you should
	 * not call reconnect() directly. Instead you should create a thread which will reconnect after
	 * a short delay. The delay should raise with unsuccessful requests to reconnect
	 *
	 *
	 * @param code reason for reconnect. If naturalNull - no reason given
	 */
	virtual void onLostConnection(natural code) {}

	///Sends text message
	/**
	 * @param msg contains text to send
	 *
	 * @note if the object is in disconnected state, message is stored and send once the
	 * object is switched back to connected state.
	 *
	 * @note there can be limit for the message. This interface doesn't support sending the
	 * message split into parts. If you need it, you can use Super::sendTextMessage without
	 * the feature storing the message during disconnected phase
	 */
	virtual void sendTextMessage(ConstStrA msg);

	///Sends binary message
	/**
	 * @param msg contains bytes to send
	 *
	 * @note if the object is in disconnected state, message is stored and send once the
	 * object is switched back to connected state.
	 *
	 * @note there can be limit for the message. This interface doesn't support sending the
	 * message split into parts. If you need it, you can use Super::sendBinMessage without
	 * the feature storing the message during disconnected phase
	 */
	virtual void sendBinMessage(ConstBin msg);

	///Sends ping
	/**
	 * @param msg payload of ping message
	 * @note if object is disconnected, no frame is sent (no exception is generated)
	 */
	virtual void sendPing(ConstBin msg);

	///Sends pong
	/**
	 * @param msg payload of pong message
	 * @note if object is disconnected, no frame is sent (no exception is generated)
	 */
	virtual void sendPong(ConstBin msg);

	///Retrieves underlying stream
	/**
	 *
	 * @return underlying stream
	 *
	 * @note You should avoid to directly write or read data from the stream. use only
	 * for detection whether data are available
	 */
	PNetworkStream getStream() const;

	///called when a text message arrives
	/**
	 * @param msg message
	 *
	 * @note function is called in context of thread which invoked the function wakeUp(). If you need
	 * to process long operation, you should to move execution to separate thread
	 *
	 */
	virtual void onTextMessage(ConstStrA msg) {(void)msg;}

	///called when a binary message arrives
	/**
	 * @param msg message
	 *
	 * @note function is called in context of thread which invoked the function wakeUp(). If you need
	 * to process long operation, you should to move execution to separate thread
	 *
	 */
	virtual void onBinaryMessage(ConstBin msg) {(void)msg;}


	///receives pong payload
	virtual void onPong(ConstBin msg) {(void)msg;}

	///Give to object chance to run for some time
	/** By default, function wakeUp is called from the NetworkEventListener. If the object
	 * is not connected to any of such an object, there is no way, how to check the connection for
	 * some new data. Function wakeUp should be called when some data has been
	 * detected on the connection. You can use for example INetworkResource::wait() for this purpose.
	 * Call getStream() to receive an instance of underlying connection
	 *
	 * @param reason
	 */
	virtual void wakeUp(natural reason = 0) throw();


protected:

	natural stream_read(byte *buffer, natural length);
	void stream_write(const byte *buffer, natural length);
	void stream_closeOutput();



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

	///called when connection has been closed by a remove site
	/**
	 * @param code reason of close. The value naturalNull means that connection has
	 * been closed unexpectly.
	 *
	 * @note you should call original handler for correct function
	 */
	virtual void onCloseOutput(natural code);



	PNetworkStream stream;
	ClientConfig cfg;
	PNetworkEventListener listener;
	StringA url;

	FastLockR lock;

	Queue<Promise<void> > queuedMsgs;


	void rearmStream();

private:
	void disconnectInternal(natural reason);
	void connectInternal();
	Future<void> onReconnect();

};
} /* namespace snapytap */

#endif /* CLIENTWS_H_ */
