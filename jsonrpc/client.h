/*
 * client.h
 *
 *  Created on: 4. 5. 2016
 *      Author: ondra
 */

#ifndef LIBS_JSONRPCSERVER_JSONRPC_CLIENT_H_
#define LIBS_JSONRPCSERVER_JSONRPC_CLIENT_H_
#include <lightspeed/base/containers/queue.h>
#include <lightspeed/base/streams/memfile.h>
#include <lightspeed/utils/json/json.h>
#include "../httpclient/httpClient.h"

#include "lightspeed/base/actions/promise.h"

#include "lightspeed/base/containers/autoArray.h"

#include "rpchandler.h"
using jsonsrv::RpcError;
using LightSpeed::MemFile;
namespace jsonrpc {

using namespace LightSpeed;

struct ClientConfig: public BredyHttpClient::ClientConfig {
	StringA url;
	JSON::PFactory jsonFactory;
};

using jsonsrv::RpcError;


class Client {
public:
	Client(const ClientConfig &cfg);

	class Result: public JSON::ConstValue {
	public:
		Result(JSON::ConstValue result, JSON::ConstValue context): JSON::ConstValue(result),context(context) {}
		JSON::ConstValue context;
	};

	///Perform RPC call asynchronously
	/**
	 *
	 * @param jsonFactory reference to JSON factory which will be used to parse response and create JSON result. Note that
	 *  factory object must be MT save, or you should avoid to access it until the result is received
	 * @param method name of the method
	 * @param params parameters, must be either array or object
	 * @param context object which contains
	 * @return function returns future which can be resolved later
	 *
	 *
	 */
	Future<Result> callAsync(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

	Result call(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);

	///Starts batch of requests
	/** When batch is opened, client starts to collect requests without sending anyone to the server. The batch must be
	 * closed to process all collected requests.
	 *
	 * The batch can be opened multiple times from different threads. Each beginBatch must have corresponding closeBatch.
	 * By opening the batch in one thread causes, that request from an other thread are also collected and sent as batch
	 * is closed.
	 *
	 * You can open batch any time, even if client currently processing a request. The last closeBatch will wait
	 * until current request is finished. During waiting it is still possible to collect other requests and also
	 * reopen a close batch. The batch is finally closed by sending and processing the request. The thread which
	 * finally closed the batch will handle all collected requests
	 *
	 * @note when batch is opened, you have to use callAsync, because call() will block. You should also avoid waiting
	 * for the Future until the batch is opened
	 *
	 * @note callAsync internally opens the batch for itself. It is possible, that function will also collect other requests
	 * while it waiting to finish some previous operation.
	 *
	 */
	void beginBatch();

	///Closes the batch
	/** @see beginBatch
	 *
	 */
	void closeBatch();

	///RIAA object to handle batches
	class Batch {
	public:
		Batch(Client &client):client(client) {client.beginBatch();}
		~Batch() {client.closeBatch();}
		Batch(const Batch &other):client(other.client) {client.beginBatch();}

		Future<Result> call(ConstStrA method, JSON::ConstValue params, JSON::ConstValue context = 0);
	protected:
		Client &client;
	};

protected:
	BredyHttpClient::HttpClient http;
	JSON::PFactory jsonFactory;

	struct PreparedItem {
		PreparedItem(const JSON::ConstValue &request,Promise<Result> result);
		~PreparedItem();
		JSON::ConstValue request;
		mutable Promise<Result> result;
		mutable bool done;
	};


	typedef AutoArray<PreparedItem> PreparedList;

	PreparedList preparedList;

	natural batchCounter;
	FastLock batchAccess;
	FastLock processingAccess;
	StringA url;
	MemFile<> serialized;

	void runBatch();

};

} /* namespace jsonrpc */

#endif /* LIBS_JSONRPCSERVER_JSONRPC_CLIENT_H_ */
