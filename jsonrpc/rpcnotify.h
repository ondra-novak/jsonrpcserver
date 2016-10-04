/*
 * rpcnotify.h
 *
 *  Created on: 4.7.2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_
#define JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_


namespace BredyHttpSrv {
class IHttpHandlerContext;
}

namespace jsonrpc {
	struct Request;

	///contains prepared notification
	/** Object has just prepared notification ready to send. Using
	 * prepared notification increases performance while notification
	 * is sent to many listeners
	 */
	class PreparedNotify {
	public:
		const StringA content;

		PreparedNotify(ConstStrA notifyName, JSON::ConstValue params, const JSON::Builder &json);
	protected:
		PreparedNotify(const StringA &content);
	};

	class IRpcNotify: public LightSpeed::IInterface {
	public:

		enum TimeoutControl {

			///Use standard timeout
			/** Notification is sent during standard processing for this client only.
			 *
			 * Not useful for broadcast because connection may block for a significant time
			 *  when clients loosing the connection.
			 */
			standardTimeout,
			///Use short timeout
			/** Notification is broadcasted through various clients.Use shortest possible
			 * timeout. You need to send short messages otherwise clients can be dropped often
			 * by a mistake
			 */
			shortTimeout,

		};


		///Prepares notification
		/**
		 * Prepared notification allows to send one notification to many subscribers. PreparedNotify
		 * object contains already serialized message ready to send directly to the websocket's channel
		 * This allows to use first subscriber to prepare notification and then use
		 * prepared notification to send it multiple subscribers.
		 *
		 * @param name name of notification
		 * @param arguments arguments
		 * @return pointer to prepared notification
		 *
		 * @note you can have only one prepared notification at time for the subscriber.
		 * You have to destroy prepared notification by unprepare(). During notification is
		 * prepared, the source connection is locked for exclusive use, only sendPrepared
		 * can be called. You have to correctly destroy prepared notification even if
		 * exception is thrown. For convince please use PrepareNotifyScope class, which is able to
		 * handle this situation.
		 *
		 */
		virtual PreparedNotify prepare(LightSpeed::ConstStrA name, LightSpeed::JSON::ConstValue arguments) = 0;


		///Send prepared notification
		/**
		 * @param prepared pointer to prepared notification
		 * @param tmControl controls timeout
		 */
		virtual void sendPrepared(const PreparedNotify &prepared, TimeoutControl tmControl = shortTimeout) = 0;


		///Sends notification
		/**
		 * @param name name of notification
		 * @param arguments arguments
		 * @param tmControl controls timeout
		 *
		 * notification is prepared and send.
		 */
		virtual void sendNotification(LightSpeed::ConstStrA name, LightSpeed::JSON::ConstValue arguments, TimeoutControl tmControl = standardTimeout) = 0;



		///Closes the websocket connection
		/** Sends "close" notificiation. It closes the websocket stream */
		virtual void closeConnection(natural code=1000) = 0;


		///Drops connection unconditionally
		/** Use function to close connection with some issue, for example if you
		 * experiencing network issues or network timeouts. Function simply closes output
		 * which should force the other side to close connection to. It also
		 * calls onClose() to notify all collectors. The connection can live for some time, however, no
		 * monitors should able to see it anymore.
		 *
		 * To close connection by standard way, use closeConnection();
		 */
		virtual void dropConnection() = 0;

		///Return future which is resolved when connection is closed
		/** You can use Future::then to specify any action. Note that
		 * during processing the handler, object is still valid and connection is available.
		 * @return Future which resolves once connection is closed
		 */
		virtual Future<void> onClose() = 0;


		static IRpcNotify *fromRequest(const Request &r);
	};


}

namespace jsonsrv {

	struct RpcRequest;
	typedef jsonrpc::PreparedNotify PreparedNotify;

	class IRpcNotify: public jsonrpc::IRpcNotify {
	public:

		virtual void setContext(BredyHttpSrv::IHttpHandlerContext *context) = 0;
		virtual BredyHttpSrv::IHttpHandlerContext *getContext() const = 0;

		///Converts request to pointer IRpcNotify
		/**
		 * @param r pointer to RpcRequest
		 * @return returns the null, if request is not from wsrpc. Otherwise a valid pointer is returned.
		 * The pointer persists with the websocket's connection and doesn't change for the client,
		 * so you can use it as identification of the connection. However, connection can
		 * be closed any time between the requests making the pointer invalid. During
		 * the request, pointer remains valid, so it is safe to send notification only during
		 * the request. If you need to send notification between the request, you have to
		 * store the pointer along with a future variable available through the onClose() function.
		 * The future variable is resolved once the connection is closed. You should install a handler
		 * to the future to make the pointer invalid. A proper synchronization (lock) is required there
		 *
		 * To create collections of websockes connections, you can use WSCollector. This will
		 * handle everything for you
		 *
		 */
		static IRpcNotify *fromRequest(RpcRequest *r);
	};

}

#endif /* JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_ */
