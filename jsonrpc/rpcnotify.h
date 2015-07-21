/*
 * rpcnotify.h
 *
 *  Created on: 4.7.2015
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_
#define JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_


namespace jsonsrv {

	class IRpcNotify: public LightSpeed::IInterface {
	public:

		///contains prepared notification
		/** Object has just prepared notification ready to send. Using
		 * prepared notification increases performance while notification
		 * is sent to many listeners
		 */
		class PreparedNtf: public LightSpeed::ConstStrA {
		protected:
			friend class IRpcNotify;
			PreparedNtf(LightSpeed::ConstStrA text):LightSpeed::ConstStrA(text) {}
		};

		///Sends notification
		/**
		 * @param name name of notification
		 * @param arguments arguments
		 *
		 * notification is prepared and send.
		 */
		virtual void sendNotification(LightSpeed::ConstStrA name, LightSpeed::JSON::PNode arguments) = 0;
		///Sends already prepared notification
		/**
		 * @param ntf prepared notification
		 */
		virtual void sendNotification(const PreparedNtf &ntf) = 0;
		///Prepares notification
		/**
		 * @param name name of notification
		 * @param arguments arguments
		 * @return prepared notification object
		 *
		 * @note that the object PreparedNtf is connected with the
		 * connection because connection object hold its data. If this object
		 * is destroyed, prepared notification becomes invalid and undefined. The
		 * same situation becomes, when another notification is prepared with
		 * the same connection object.
		 *
		 * Function is intended for sending message to multiple connection. Just use
		 * first connection to create prepared notification and immediatelly
		 * broadcast the message to the listeners. Then you should destroy the
		 * prepared notification or leave on the stack.
		 */
		virtual PreparedNtf prepareNotification(LightSpeed::ConstStrA name, LightSpeed::JSON::PNode arguments) = 0;

		virtual void setContext(IHttpHandlerContext *context) = 0;
		virtual IHttpHandlerContext *getContext() const = 0;

		///Closes the websocket connection
		/** Sends "close" notificiation. It closes the websocket stream */
		virtual void closeConnection(natural code=1000) = 0;

		static IRpcNotify *fromRequest(RpcRequest *r);



	protected:
		static PreparedNtf prepare(ConstStrA msg) {return msg;}

	};

}



#endif /* JSONRPCSERVER_JSONRPC_RPCNOTIFY_H_ */
