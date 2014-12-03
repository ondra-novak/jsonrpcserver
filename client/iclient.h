/*
 * iclient.h
 *
 *  Created on: Nov 20, 2012
 *      Author: ondra
 */

#ifndef JSONRPC_ICLIENT_H_
#define JSONRPC_ICLIENT_H_

#include "lightspeed/base/exceptions/genexcept.h"
#include "../jsonrpc/rpchandler.h"

namespace jsonrpc {

using jsonsrv::RpcError;
using namespace LightSpeed;


class IClient {
public:

	virtual ~IClient() {}

	typedef natural Ticket;

	///Invokes one method call
	/**
	 * This is very simple way to call method, but also very ineffective when
	 * multiple calls are need. Function build query, sends request and reads
	 * response. All operations are synchronous, you cannot schedule sending
	 * and receiving.
	 *
	 * @param methodName method name
	 * @param arrayParams array of parameters. Must be array, otherwise error reported
	 * @param setError pointer to variable, which receives information whether
	 * 			returned value is valid result or error. If this argument
	 * 			is set to 0, only valid results are returned and errors
	 * 			are thrown as exception
	 * @return result or error, depend on setError argument
	 */

	virtual JSON::PNode call(ConstStrA methodName, JSON::PNode arrayParams, bool *setError = 0) = 0;
	///Calls method asynchronous
	/** Enqueues method into output buffer. Function doesn't handle receiving
	 * at background, but you can easy to control it by own. In signle request
	 * you can enqueue multiple methods. Every method receives result which
	 * can be later retrieved using function getResult()
	 *
	 * @param methodName method name
	 * @param arrayParams array of parameters. Must be array, otherwise error is reported
	 * @return function returns ticket identifier. Ticked identifies response
	 * and is need by function getResult().
	 */
	virtual Ticket callAsync(ConstStrA methodName, JSON::PNode arrayParams) = 0;
	///Sends notify to the server
	/** Notification is message without reply.
	 *
	 * @param methodName method name which receives notification
	 * @param arrayParams array of parameters. Must be array, otherwise error is reported
	 * @param async default value is true causing, that notification is
	 * send asynchronous (same way as callAsync). If you set this to the
	 * false, notification is sent before method finish. If there
	 * are other request enqueued to the async queue, they are sent too
	 */
	virtual void sendNotify(ConstStrA methodName, JSON::PNode arrayParams, bool async=true) = 0;

	///Retrieves result stored under ticket.
	/**
	 * @param ticket identifier of result
	 * @param setError pointer to variable, which receives information whether
	 * 			returned value is valid result or error. If this argument
	 * 			is set to 0, only valid results are returned and errors
	 * 			are thrown as exception
	 * @return result or error, depend on setError argument
	 *
	 * @note function clear stored result before exits making ticked invalid.
	 * Function additionally calls sendAndReceive() function if it is
	 * necessary.
	 *
	 * When invalid ticket is used, function throws an exception
	 */

	virtual JSON::PNode getResult(Ticket ticket, bool *setError) = 0;
	///Returns true, if ticket is valid
	/**
	 * @param ticket ticket
	 * @retval true ticket is valid
	 * @retval false ticket is not valid, it doesn't contain a result.
	 * Note that all ticket appear invalid before request is sent
	 */
	virtual bool isTicketValid(Ticket ticket) const = 0;
	///Clears all stored results. All ticker becomes invalid
	virtual void clearAllResults() = 0;
	///Clear one result
	virtual void clearResult(Ticket ticket) = 0;
	///Determines, whether there are stored notifications
	/**
	 * @retval true yes
	 * @retval false no
	 */
	virtual bool hasNotify() const = 0;
	///Retrieves next notification
	virtual JSON::PNode getNotify(bool *setError = 0) = 0;

	///Modifies context of client.
	/**
	 * @param contextName name of context variable
	 * @param context context data, if set to nil, value is removed (nil, not NULL in JSON)
	 */
	virtual void setContext(ConstStrA contextName, JSON::PNode context) = 0;
	///Gets client context variable
	/**
	 * @param contextName context variable name
	 * @return value of context, nil of variable not defined
	 */
	virtual JSON::PNode getContext(ConstStrA contextName) const = 0;
	///process all context variables
	virtual bool enumContext(const JSON::IEntryEnum &enumCb) = 0;

	virtual void clearContext() = 0;

	///Sends prepared request.
	/**
	 * Sends request to the output stream, but doesn't wait for response.
	 * If there is no ready request, function does nothing. If function can call
	 * receiveReply() before new request is sent.
	 *
	 * @note all stored replies and notifications are deleted before new request is sent. If
	 * function must call receiveReply() function, results of this call are not deleted
	 */
	virtual void sendRequest() = 0;
	///Receives reply from server
	/**
	 * Function receives reply reading input stream. This can cause blocking operation.
	 * If you plan to perform non-blocking reading, you have to implement
	 * buffered stream, pre-read data to the buffer and then receiveReply from the
	 * buffer.
	 *
	 * If client doesn't expect reply, it does nothing
	 */
	virtual void receiveReply() = 0;
	///Perform both operations - send and reply
	virtual void sendAndReceive() = 0;


	virtual JSON::IFactory *getFactory() = 0;

};
extern const char *exception_InvalidClientTicket;
typedef GenException1<exception_InvalidClientTicket, IClient::Ticket> InvalidClientTicketException;

}



#endif /* JSONRPC_ICLIENT_H_ */
