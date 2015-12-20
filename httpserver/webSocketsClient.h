#pragma once

#include "lightspeed/base/containers/constStr.h"
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/streams/netio.h"
#include "lightspeed/base/actions/promise.h"
#include "abstractWebSockets.h"

namespace BredyHttpSrv {


	///Abstract Websockets server connection. Class also works as handler for websocket events
	/**
	* To use this class, inherit it. You will also need to inherit AbstractWebSocketsHandler
	* as factory of instance of this class.
	*
	* @note MT-safety: Object is not MT safe. Before sending any frame, you should
	* acqure lock protecting this object agains reentering. This also includes
	* Writing from event function
	*/
	class WebSocketClientConnection : public AbstractWebSocketConnection<WebSocketClientConnection, false> {
	public:
		typedef AbstractWebSocketConnection<WebSocketClientConnection, false> Super;


		///Called when connection is established
		/** Default implementation has empty body. */
		virtual void onConnect();
		///Called when text message arrives
		virtual void onTextMessage(ConstStrA msg);
		///Called when binary message arrives
		virtual void onBinaryMessage(ConstBin msg);
		///Called when close request arrived.
		virtual void onCloseOutput(natural code);
		///Called when pong packet arrives
		virtual void onPong(ConstBin msg);

		///Call this function when any data incomes to the original network stream;
		virtual bool onRawDataIncome();

		///Constructs websocket connection
		/**
		* @param request reference to IHttpRequest available during initial handshake
		*/
		WebSocketClientConnection(PNetworkStream &stream);


		struct HeaderLine {
			ConstStrA field;
			ConstStrA value;
		};

		///connects to server
		/** Call function to send HTTP request to initiate websocket connection.
		Function returns Promise, which becomes resolved once connection is established
		*/

		Promise<void> connectToServer(ConstStrA host, ConstStrA path, ConstStrA method = ConstStrA(), ConstStringT<HeaderLine> headers = ConstStringT<HeaderLine>());

		typedef NetworkStream<2048> Stream;

		Stream &getStream();

		class AlreadyConnected : public Exception {
		public:
			LIGHTSPEED_EXCEPTIONFINAL;
			AlreadyConnected(const ProgramLocation &loc) :Exception(THISLOCATION) {}
		protected:
			virtual void message(ExceptionMsg &msg) const;
		};

	protected:

		Stream stream;
		bool connected;
		typedef Map<StringPoolA::Str, StringPoolA::Str> Header;
		StringPoolA strpool;
		Header hdrs;
		natural status;
		Optional<Promise<void>::Result > connectResolution;


	private:
		natural stream_read(byte *buffer, natural length);
		void stream_write(const byte *buffer, natural length);
		void stream_closeOutput();

		StringA createWSKey();
		bool processResponse(natural sep);

	};

}