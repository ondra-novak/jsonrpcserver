/*
 * abstractWebSockets.tcc
 *
 *  Created on: 4.5.2014
 *      Author: ondra
 */

#ifndef BREDY_JSONRPCSERVER_ABSTRACTWEBSOCKETS_TCC_
#define BREDY_JSONRPCSERVER_ABSTRACTWEBSOCKETS_TCC_
#include "abstractWebSockets.h"
#include <lightspeed/base/streams/random.h>
#include <lightspeed/mt/threadId.h>

namespace BredyHttpSrv {

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::sendTextMessage(
		ConstStrA msg, bool fin) {
	sendFrame(true,opcodeTextFrame,ConstBin((const byte *)msg.data(), msg.length()));


}

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::sendBinMessage(
		ConstBin msg, bool fin) {
	sendFrame(true,opcodeBinaryFrame,msg);
}

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::closeConnection(
		natural code) {

	requestClose = true;
	byte data[2];
	data[0] = byte(code / 256);
	data[1] = byte(code % 256);
	sendFrame(true,opcodeConnClose,ConstBin(data,2));
	this->_invoke().stream_closeOutput();

}

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::sendPing(
		ConstBin msg) {
	sendFrame(true,opcodePing, msg);
}

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::sendPong(
		ConstBin msg) {
	sendFrame(true,opcodePong, msg);
}

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::sendFrame(
		bool final, byte opcode, ConstBin msg) {
	byte frameHeader[20];
	frameHeader[0] = (final?0x80:0x00) | opcode;
	natural frameLen = 2;
	lnatural len = msg.length();
	if (len < 126) {
		frameHeader[1] = (byte)(msg.length() & 0x7F);
	} else if (len < 65536) {
		frameHeader[1] = 126;;
		frameHeader[2] = (byte)(len >> 8);
		frameHeader[3] = (byte)(len & 0xFF);
		frameLen = 4;
	} else {
		frameHeader[1] = 127;
		for (natural i = 0; i < 8; i++) {
			frameHeader[9-i] = (len >> (i * 8)) & 0xFF;
		}
		frameLen = 10;
	}
	if (!serverSide) {
		frameHeader[1] |= 0x80;
		byte *mask = frameHeader+frameLen;
		frameLen+=4;
	/*	mask[0] = (byte)rand->getNext();
		mask[1] = (byte)rand->getNext();
		mask[2] = (byte)rand->getNext();
		mask[3] = (byte)rand->getNext();*/
		this->_invoke().stream_write(frameHeader,frameLen);
		sendBytesMasked(msg.data(), msg.length(), mask);
	} else {
		this->_invoke().stream_write(frameHeader,frameLen);
		this->_invoke().stream_write(msg.data(),msg.length());
	}
}

template<typename Impl, bool serverSide>
bool AbstractWebSocketConnection<Impl,serverSide>::onRawDataIncome() {
	if (requestClose) return false;
	natural avail = frame.getAllocatedSize() - frame.length();
	if (avail == 0) avail = 256;
	natural pos = frame.length();
	frame.resize(pos + avail);
	natural rd = this->_invoke().stream_read(frame.data() + pos, avail);
	if (rd == 0) return false;
	frame.resize(pos + rd);

	natural payloadLen = 0;
	natural beginOfFrame = 0;
	byte *maskKey = 0;

	if (frame.length() < 2)
		return true;
	byte b = frame[1] & 0x7F;
	if (b < 126) {
		payloadLen = b;
		beginOfFrame = 2;
	} else if (b == 126) {
		if (frame.length() < 4)
			return true;
		payloadLen = (natural) frame[2] * 256 + frame[3];
		beginOfFrame = 4;
	} else if (b == 127) {
		if (frame.length() < 10)
			return true;
		for (int i = 0; i < 8; i++)
			payloadLen = (payloadLen << 8) + frame[i + 2];
		beginOfFrame = 10;
	}
	if (frame[1] & 0x80) {
		//extract mask key;
		if (frame.length() < beginOfFrame + 4)
			return true;
		maskKey = frame.data() + beginOfFrame;
		beginOfFrame += 4;
	}
	if (payloadLen > maxMessageSize) {
		closeConnection(closeMessageTooBig);
		return false;
	}
	if (frame.length() < beginOfFrame + payloadLen) {
		frame.reserve(beginOfFrame + payloadLen);
		return true;
	}
	if (maskKey) { 
		for (natural i = 0; i < payloadLen; i++) {
			frame(beginOfFrame + i) ^= maskKey[(i & 0x3)];
		}
	}
	natural frameLen = beginOfFrame + payloadLen;
	ConstBin payload = frame.mid(beginOfFrame, payloadLen);

	byte opcode = frame[0] & 0xF;
	bool final = (frame[0] & 0x80) != 0;

	switch (opcode) {
	case opcodeConnClose:
		if (payloadLen < 2) {
			this->_invoke().onCloseOutput(1000);
			closeConnection(1000);
		} else {
			natural status = (natural(payload[0]) << 8) + payload[1];
			this->_invoke().onCloseOutput(status);
			closeConnection(status);
		}
		return false;

	case opcodePing: sendPong(payload);break;
	case opcodePong: this->_invoke().onPong(payload);break;
	case opcodeContFrame: {
			if (fragmentedMsgType == 0) {
				closeConnection(closeProtocolError);
				return false;
			}
			if (fragmentBuffer.length() + payload.length() > maxFragmentedMessageSize) {
				closeConnection(closeMessageTooBig);
				return false;
			}
			fragmentBuffer.append(payload);
			if (final) {
				deliverPayload(fragmentedMsgType, fragmentBuffer);
				fragmentedMsgType = 0;
				fragmentBuffer.clear();
			}
		}
						  break;
	case opcodeTextFrame:
	case opcodeBinaryFrame:
			if (final) {
				deliverPayload(opcode, payload);
			} else {
				if (payload.length() > maxFragmentedMessageSize) {
					closeConnection(closeMessageTooBig);
					return false;
				}
				fragmentedMsgType = opcode;
				fragmentBuffer.clear();
				fragmentBuffer.append(payload);
			}
			break;
	default: closeConnection(closeProtocolError);
			return false;
	}

	frame.erase(0,frameLen);
	return !requestClose;
}

template<typename Impl, bool serverSide>
ConstStrA AbstractWebSocketConnection<Impl,serverSide>::getProtocolName() const {
	return ConstStrA();
}

template<typename Impl, bool serverSide>
inline void AbstractWebSocketConnection<Impl,serverSide>::sendBytesMasked(const byte* b, natural len , const byte *maskKey) {
	while (len > 1024) {
		sendBytesMasked(b,1024,maskKey);
		b+=1024;
		len-=1024;
	}
	if (len != 0) {
		byte *c = (byte *)alloca(len);
		for (natural i = 0; i < len; i++) {
			c[i] = b[i] ^ maskKey[i & 0x3];
		}
		this->_invoke().stream_write(c,len);
	}
}

template<typename Impl, bool serverSide>
inline AbstractWebSocketConnection<Impl,serverSide>::AbstractWebSocketConnection()
:maxMessageSize(naturalNull),maxFragmentedMessageSize(naturalNull),requestClose(false)
{
	if (!serverSide) {
		uint32_t randomData[32];
		uint32_t *randomData2 = new uint32_t[32];
		uint32_t seed = (uint32_t)ThreadId::current().asAtomic() + TimeStamp::now().getTime();
		for (natural i = 0; i < 32; i++) {
			seed += randomData[i] + randomData2[i];
		}
		delete [] randomData2;

//		rand = new Rand(seed);
	} else {
		rand = 0;
	}
}

template<typename Impl, bool serverSide>
inline AbstractWebSocketConnection<Impl,serverSide>::~AbstractWebSocketConnection()
{
//	if (!serverSide) delete rand;
}

template<typename Impl, bool serverSide>
void AbstractWebSocketConnection<Impl,serverSide>::deliverPayload(
		byte opcode, ConstBin data) {

	if (opcode == opcodeBinaryFrame) {
		this->_invoke().onBinaryMessage(data);
	} else {
		this->_invoke().onTextMessage(ConstStrA((const char *)data.data(),data.length()));
	}
}



}

#endif /* BREDY_JSONRPCSERVER_ABSTRACTWEBSOCKETS_TCC_ */
