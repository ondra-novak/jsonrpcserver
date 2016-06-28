/*
 * abstractWebSockets.h
 *
 *  Created on: 4.5.2014
 *      Author: ondra
 */

#ifndef BREDY_JSONRPCSERVER_ABSTRACTWEBSOCKETS_H_
#define BREDY_JSONRPCSERVER_ABSTRACTWEBSOCKETS_H_
#include "lightspeed/base/invokable.h"
#include "lightspeed/base/containers/autoArray.h"
#include "lightspeed/base/containers/constStr.h"
#include "lightspeed/base/memory/allocPointer.h"



namespace BredyHttpSrv {

using namespace LightSpeed;

///Some constants defined for websockets
class WebSocketsConstants {
public:
	static const unsigned int opcodeContFrame = 0;
	static const unsigned int opcodeTextFrame = 1;
	static const unsigned int opcodeBinaryFrame = 2;
	static const unsigned int opcodeConnClose = 8;
	static const unsigned int opcodePing = 9;
	static const unsigned int opcodePong = 10;

	static const unsigned int closeNormal = 1000;
	static const unsigned int closeGoingAway = 1001;
	static const unsigned int closeProtocolError = 1002;
	static const unsigned int closeUnsupportedData = 1003;
	static const unsigned int closeNoStatus = 1005;
	static const unsigned int closeAbnormal = 1006;
	static const unsigned int closeInvalidPayload = 1007;
	static const unsigned int closePolicyViolation = 1008;
	static const unsigned int closeMessageTooBig = 1009;
	static const unsigned int closeMandatoryExtension = 1010;
	static const unsigned int closeInternalServerError = 1011;
	static const unsigned int closeTLSHandshake = 1015;

};

///Generates masking bytes
/** It required that client must generate random masking bytes. You can supply own
random generator by implementing this interface. There are two predefined generators

WebSocketMaskingSecure - it uses secure random generator.

WebSocketMaskingDisable - it disables masking without breaking the protocol rules. This
						is intended for debug purposes, for example, if you need
						to monitor connection using packet capturing service. However,
						this implementation should not be used in the production.
     
*/
class IWebSocketMasking {
public:
	///sets 4 bytes at address depend on implementation
	virtual void setMaskingBytes(byte *mask) = 0;
	virtual ~IWebSocketMasking() {}

	///creates fake random generator which initializes masking bytes to zero
	/**
	@note object is allocated. You have to delete it after usage, or use proper smart pointer
	*/
	static IWebSocketMasking *getDisabledMasking();
	static IWebSocketMasking *getDisabledMasking(IRuntimeAlloc &alloc);
	///creates secure random generator for masking
	/** 
	 @note Generator can be resource intensive because it uses OS's random generator.
	 If you not requests perfect security (because content is never generated by a user,
	    you can use getFastMasking() 
		
	@note object is allocated. You have to delete it after usage, or use proper smart pointer

	*/
	static IWebSocketMasking *getSecureMasking();
	static IWebSocketMasking *getSecureMasking(IRuntimeAlloc &alloc);
	
	///creates xorshift64* generator to generate masking bytes. 
	/** It uses SecureRandom to seed the generator. This is default, if no generator specified 
	
	@note object is allocated. You have to delete it after usage, or use proper smart pointer

	*/
	static IWebSocketMasking *getFastMasking();
	static IWebSocketMasking *getFastMasking(IRuntimeAlloc &alloc);
};


///Template that helps to build various websocket adapters
/**
 * @tparam Impl class, that implements stream functions. Must be name of class, that inherits this template
 * @tparam serverSide set true, if class implements server side of websockets. Set false for class implementing
 *   client side. This affect frame masking (server doesn't mask frames, client does)
 */
template<typename Impl, bool serverSide>
class AbstractWebSocketConnection: public Invokable<Impl>, public WebSocketsConstants {
public:
	///Called when text message arrives
	/**
	 * @param msg arrived message
	 *
	 * Default implementation closes connection with "closeInvalidPayload"
	 */
	void onTextMessage(ConstStrA msg);
	///Called when binary message arrives
	/**
	 * @param msg arrived message
	 *
	 * Default implementation closes connection with "closeInvalidPayload"
	 */
	void onBinaryMessage(ConstBin msg);
	///Called when close request arrived.
	/**
	 * @param code status code
	 *
	 * Function don't need to call closeConnection in reaction to this event.
	 * Connection is closed automatically after function returns
	 *
	 * Default implementation has empty body
	 */
	void onCloseOutput(natural code);
	///Called when pong packet arrives
	/**
	 * @param msg data from pong
	 *
	 * Default implementation has empty body
	 */
	void onPong(ConstBin msg);

	///Sends text message through websocket
	/**
	 * @param msg message
	 * @param fin set true, if this message is final. Set false, if this is not final packet
	 *
	 * Note you should not allow to other thread send messages until you finish fragmented message
	 */
	void sendTextMessage(ConstStrA msg, bool fin = true);
	///Sends binary message through websocket
	/**
	 * @param msg message
	 * @param fin set true, if this message is final. Set false, if this is not final packet
	 *
	 * Note you should not allow to other thread send messages until you finish fragmented message
	 */
	void sendBinMessage(ConstBin msg, bool fin = true);
	///Requests to close socket
	/** @param code status code */
	void closeConnection(natural code = 1000);
	/// Requests ping
	/**
	 * @param msg ping data
	 */
	void sendPing(ConstBin msg);
	/// Sends pong
	/**
	 * This allows to send unsolicited pong message. You don't need to write
	 * function to reply ping message they are replied automatically
	 * @param msg pong data
	 */
	void sendPong(ConstBin msg);


	///Sends frame manually
	/**
	 * @param final frame is last in chain
	 * @param opcode opcode
	 * @param msg payload
	 */
	void sendFrame(bool final, byte opcode,ConstBin msg);

	void sendBytesMasked(const byte *b, natural len, const byte *maskKey);


	///Call this function repeatedly while there are data to read
	/**
	 * @retval true processed
	 * @retval false connection has been closed by remote site
	 */

	bool onRawDataIncome();

	///Retrieves name of protocol used by this connection
	/** Protocol name is optional and it is retrieved during connection handshake
	 *  to include correct protocol name into header. When function returns
	 *  empty string, no protocol information is included
	 * @return name of protocol or empty string
	 */
	ConstStrA getProtocolName() const;


	///Changes masking function
	/** Function is intended to be useful only at client side, you don't need to set this
	on server side, because server should not mask the output stream. At client side,
	first frame initalizes maskingFn using function getFastMasking() unless you specify
	own version. For debugging purposes, you can initialize function 
	using the getDisabledMasking()

	@param maskingFn pointer to instance implementing desired interface. Note that object
	takes ownership and deletes the instance during its destruction. Also this function
	deletes previous implementation

	*/
	void setMaskingFn(IWebSocketMasking *maskingFn);

	AbstractWebSocketConnection();
	~AbstractWebSocketConnection();

protected:

	AutoArray<byte> frame;
	AutoArray<byte> fragmentBuffer;

	natural maxMessageSize;
	natural maxFragmentedMessageSize;
	byte fragmentedMsgType;

	AllocPointer<IWebSocketMasking> maskingFn;
	bool requestClose;

	void deliverPayload(byte opcode,ConstBin data);

private:
	///Implementation must define body of this function
	/**
	 * @param buffer buffer for data
	 * @param length length of that buffer
	 * @return function returns count bytes read from the stream. If there are no more data (connection closed)
	 * function have to return zero. If function is called while data are not ready to read, function should
	 * block until at least one byte is ready to read.
	 *
	 */
	natural stream_read(byte *buffer, natural length);
	///Implementation must write buffer into stream
	/**
	 * @param buffer buffer to write
	 * @param length length count of bytes
	 *
	 * @note function have to write all bytes or throw an eception
	 */
	void stream_write(const byte *buffer, natural length);
	///Closes output of the stream.
	void stream_closeOutput();


	AbstractWebSocketConnection(const AbstractWebSocketConnection &other);
	AbstractWebSocketConnection &operator=(const AbstractWebSocketConnection &other);
};

}

#endif /* BREDY_JSONRPCSERVER_ABSTRACTWEBSOCKETS_H_ */
