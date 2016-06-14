/*
 * rpcnotify.h
 *
 *  Created on: 4.7.2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_
#define JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_


namespace jsonsrv {

	class PreparedNotify;
	class IRpcNotify: public LightSpeed::IInterface {
	public:

		///contains prepared notification
		/** Object has just prepared notification ready to send. Using
		 * prepared notification increases performance while notification
		 * is sent to many listeners
		 */
		friend class PreparedNotify;

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
		virtual PreparedNotify *prepare(LightSpeed::ConstStrA name, LightSpeed::JSON::PNode arguments) = 0;


		///Send prepared notification
		/**
		 * @param prepared pointer to prepared notification
		 */
		virtual void sendPrepared(const PreparedNotify *prepared) = 0;


		///Destroyes prepared notification
		/**
		 * @param prepared pointer to prepared notification.
		 *
		 * You have to call this function when object is no longer needed. Note that
		 * until the notification is destroyed, the connection is locked.
		 */
		virtual void unprepare(PreparedNotify *prepared) throw() = 0;
		///Sends notification
		/**
		 * @param name name of notification
		 * @param arguments arguments
		 *
		 * notification is prepared and send.
		 */
		virtual void sendNotification(LightSpeed::ConstStrA name, LightSpeed::JSON::PNode arguments) = 0;

		virtual void setContext(IHttpHandlerContext *context) = 0;
		virtual IHttpHandlerContext *getContext() const = 0;

		///Closes the websocket connection
		/** Sends "close" notificiation. It closes the websocket stream */
		virtual void closeConnection(natural code=1000) = 0;

		static IRpcNotify *fromRequest(RpcRequest *r);

		///Return future which is resolved when connection is closed
		/** You can use Future::then to specify any action. Note that
		 * during processing the handler, object is still valid and connection is available.
		 * @return Future which resolves once connection is closed
		 */
		virtual Future<void> onClose() = 0;
	};


	class PreparedNotifyScope {
	public:
		PreparedNotifyScope(PreparedNotify *ntf, IRpcNotify *obj)
			:ntf(ntf),obj(obj) {}
		PreparedNotifyScope(IRpcNotify *firstSubscriber, ConstStrA name, LightSpeed::JSON::Value arguments)
			:ntf(firstSubscriber->prepare(name, arguments)), obj(firstSubscriber) {}
		~PreparedNotifyScope() throw() {
			obj->unprepare(ntf);
		}
		operator PreparedNotify *() {return ntf;}
		operator const PreparedNotify *() const {return ntf;}
	protected:
		PreparedNotify *ntf;
		IRpcNotify *obj;
	};
}



#endif /* JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_ */
