/*
 * webSockets.h
 *
 *  Created on: 30. 4. 2014
 *      Author: ondra
 */

#ifndef BREDY_JSONRPCSERVER_WEBSOCKETS_H_
#define BREDY_JSONRPCSERVER_WEBSOCKETS_H_

#include "httprequest.h"
#include "lightspeed/base/containers/autoArray.h"
#include "abstractWebSockets.h"

namespace BredyHttpSrv {



///Abstract Websocket connection. Class also works as handler for websocket events
/**
 * To use this class, inherit it. You will also need to inherit AbstractWebSocketsHandler
 * as factory of instance of this class.
 *
 * @note MT-safety: Object is not MT safe. Before sending any frame, you should
 * acqure lock protecting this object agains reentering. This also includes
 * Writing from event function
 */
class WebSocketConnection: public IHttpHandlerContext, public AbstractWebSocketConnection<WebSocketConnection, true> {
public:



	///Called when connection is estabilished
	/** Default implementation has empty body. */
	virtual void onConnect();
	///Called when text message arrives
	/**
	 * @param msg arrived message
	 *
	 * Default implementation closes connection with "closeInvalidPayload"
	 */
	virtual void onTextMessage(ConstStrA msg);
	///Called when binary message arrives
	/**
	 * @param msg arrived message
	 *
	 * Default implementation closes connection with "closeInvalidPayload"
	 */
	virtual void onBinaryMessage(ConstBin msg);
	///Called when close request arrived.
	/**
	 * @param code status code
	 *
	 * Function don't need to call closeConnection in reaction to this event.
	 * Connection is closed automatically after function returns
	 *
	 * Default implementation has empty body
	 */
	virtual void onCloseOutput(natural code);
	///Called when pong packet arrives
	/**
	 * @param msg data from pong
	 *
	 * Default implementation has empty body
	 */
	virtual void onPong(ConstBin msg);


	///Constructs websocket connection
	/**
	 * @param request reference to IHttpRequest available during initial handshake
	 */
	WebSocketConnection(IHttpRequest &request);



protected:

	PNetworkStream stream;

private:
	natural stream_read(byte *buffer, natural length);
	void stream_write(const byte *buffer, natural length);
	void stream_closeOutput();

	friend class AbstractWebSocketConnection<WebSocketConnection, true>;

};




///Abstract websocket handler
/**
 * Websocket handler is standard IHTTPHandler which can be registered as
 * site through addSite() function. Object handles all incoming websocket
 * connections, performing upgrade and switching protocols and then
 * creates instance for every connection.
 *
 * To build own handle, inherit this object and implement onNewConnection() function
 * which should allocate instance of AbstractWebSocketConn
 */
class AbstractWebSocketsHandler: public IHttpHandler {
public:


	/// Abstract function called when new connection arriver
	/**
	 * Main task of this function is to create instance of AbstractWebSocketConnection and return ite
	 *
	 * @param alloc new object should use specified allocator (use new(alloc) )
	 * @param request that identifies this connection
	 * @param vpath virtual path used in HTTP request
	 * @return function should return pointer to new connection, or nil, if it don't want to accept this connection
	 *
	 * @note in this phase, connection is not yet upgrated. So newly created connection should not perform
	 * send operation. Use AbstractWebSocketConn::onConnect function to send your first message
	 *
	 * Function can throw HttpStatusException to throw status code when it rejects connection. Returning nil
	 * cause that code 404 will be emited.
	 */
	virtual WebSocketConnection * onNewConnection(IRuntimeAlloc &alloc, IHttpRequest &request, ConstStrA vpath) = 0;



protected:


	virtual natural onRequest(IHttpRequest &request, ConstStrA vpath) ;
	virtual natural onData(IHttpRequest &request);
};

///Template to easy build WebSocketHandler
/**
 * @tparam WebSocketHandler class that inherits AbstractWebSocketConn
 * @tparam Args arguments passed to the constructor of WebSocketHandler
 *
 *
 */
template<typename WSConnClass, typename Args>
class WebSocketHandlerT: public AbstractWebSocketsHandler {
public:
	WebSocketHandlerT(const Args &args):args(args) {}
	virtual WebSocketConnection *onNewConnection(IRuntimeAlloc &alloc, IHttpRequest &request, ConstStrA) {
		return new(alloc) WSConnClass(request,args);
	}
protected:
	Args args;
};

///Template to easy build WebSocketHandler
/**
 * @tparam WebSocketHandler class that inherits AbstractWebSocketConn
 * @tparam Args arguments passed to the constructor of WebSocketHandler
 *
 *
 */
template<typename WSConnClass>
class WebSocketHandlerT<WSConnClass, void>: public AbstractWebSocketsHandler {
public:
	virtual WebSocketConnection *onNewConnection(IRuntimeAlloc &alloc, IHttpRequest &request, ConstStrA ) {
		return new(alloc) WSConnClass(request);
	}
};

} /* namespace BredyHttpSrv */

#endif /* BREDY_JSONRPCSERVER_WEBSOCKETS_H_ */
