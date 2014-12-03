/*
 * client.h
 *
 *  Created on: Nov 20, 2012
 *      Author: ondra
 */

#ifndef JSONRPC_CLIENT_H_
#define JSONRPC_CLIENT_H_
#include "clientAdapter.h"
#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/base/memory/rtAlloc.h"
#include "lightspeed/base/containers/autoArray.h"

#pragma once

namespace jsonrpc {

using namespace LightSpeed;

class Client: public ClientAdapter {
public:
	Client();
	Client(IRuntimeAlloc &alloc);
	Client(const SeqFileOutput &request, const SeqFileInput &response);
	Client(const PSeqFileHandle &netstream);
	Client(const SeqFileOutput &request, const SeqFileInput &response, IRuntimeAlloc &alloc);
	Client(const PSeqFileHandle &stream, IRuntimeAlloc &alloc);

	void setTarget(const SeqFileOutput &request, const SeqFileInput &response);
	void setTarget(const PSeqFileHandle &netstream);


	JSON::PNode call(ConstStrA methodName, JSON::PNode arrayParams, bool *setError = 0);
	Ticket callAsync(ConstStrA methodName, JSON::PNode arrayParams);
	void sendNotify(ConstStrA methodName, JSON::PNode arrayParams, bool async=true);

	void sendRequest();
	void receiveReply();
	void sendAndReceive();

protected:

	struct Counter {
		natural value;
		Counter():value(0) {}
	};

	SeqFileOutput request;
	SeqFileInput response;
	AutoArray<JSON::PNode,RTAlloc> outputQueue;
	Counter requestCount;
	Counter requestCountPending;

	enum State {

	};

};

} /* namespace jsonrpc */
#endif /* JSONRPC_CLIENT_H_ */
