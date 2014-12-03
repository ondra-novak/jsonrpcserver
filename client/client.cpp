/*
 * client.cpp
 *
 *  Created on: Nov 20, 2012
 *      Author: ondra
 */

#include "client.h"
#include "lightspeed/base/containers/autoArray.tcc"

namespace jsonrpc {

Client::Client()
	:request(nil),response(nil) {}

Client::Client(IRuntimeAlloc& alloc)
	:ClientAdapter(alloc),request(nil),response(nil),outputQueue(RTAlloc(alloc)) {
}

Client::Client(const SeqFileOutput& request, const SeqFileInput& response)
	:request(request),response(response)
{
}

Client::Client(const PSeqFileHandle& stream)
	:request(stream),response(stream) {
}

Client::Client(const SeqFileOutput& request, const SeqFileInput& response,
		IRuntimeAlloc& alloc)
	:ClientAdapter(alloc),request(request),response(response),outputQueue(RTAlloc(alloc)) {
}

Client::Client(const PSeqFileHandle& stream, IRuntimeAlloc& alloc)
	:ClientAdapter(alloc),request(stream),response(stream),outputQueue(RTAlloc(alloc)) {
}

void Client::setTarget(const SeqFileOutput& request, const SeqFileInput& response) {
	this->request = request;
	this->response = response;
}

void Client::setTarget(const PSeqFileHandle& netstream) {
	this->request = SeqFileOutput(netstream);
	this->response = SeqFileInput(netstream);
}

JSON::PNode Client::call(ConstStrA methodName, JSON::PNode arrayParams, bool* setError) {
	Ticket t = callAsync(methodName,arrayParams);
	sendAndReceive();
	return getResult(t,setError);
}

Client::Ticket Client::callAsync(ConstStrA methodName, JSON::PNode arrayParams) {
	if (arrayParams == nil) return callAsync(methodName,factory->array());
	if (arrayParams->getType() != JSON::ndArray) return callAsync(methodName,factory->array()->add(arrayParams));
	JSON::PNode rq = factory->object();
	Ticket t = allocTicket();
	rq->add("method",factory->newValue(methodName))
	  ->add("params",arrayParams.getMT())
	  ->add("id",factory->newValue(t));
	if (context != nil) {
		rq->add("context",context);
	}
	outputQueue.add(rq);
	requestCount.value++;
	return t;
}

void Client::sendNotify(ConstStrA methodName, JSON::PNode arrayParams, bool async) {
	if (arrayParams == nil) return sendNotify(methodName,factory->array(),async);
	if (arrayParams->getType() != JSON::ndArray) return sendNotify(methodName,factory->array()->add(arrayParams),async);
	JSON::PNode rq = factory->object();
	rq->add("method",factory->newValue(methodName))
	  ->add("params",arrayParams.getMT())
	  ->add("id",factory->newNullNode());
	if (context != nil) {
		rq->add("context",context);
	}
	outputQueue.add(rq);
}


void Client::sendRequest() {
	clearAllResults();
	if (requestCountPending.value != 0) {
		receiveReply();
	}
	for (natural i = 0; i < outputQueue.length(); i++) {
		factory->toStream(*(outputQueue[i]),request);
	}
	outputQueue.clear();
	requestCountPending = requestCount;
	requestCount.value = 0;
}

void Client::receiveReply() {
	for (natural i = 0; i < requestCountPending.value; i++) {
		bool rep;
		do {
			JSON::PNode nd = factory->fromStream(response);
			RPCResult res;
			res.data = nd->getVariable("error");
			if (res.data == nil || res.data->isNull()) {
				res.data = nd->getVariable("result");
				res.isError = false;
			} else {
				res.isError = true;
			}

			const JSON::INode *ind = nd->getVariable("id");
			if (ind == 0 || ind->isNull()) {
				rep = true;
				notifications.add(res);
			} else {
				rep = false;
				results.insert(nd->getInt(),res);
			}
		} while (rep);
	}
}

void Client::sendAndReceive() {
	sendRequest();
	receiveReply();
}
 /* namespace jsonrpc */
}
