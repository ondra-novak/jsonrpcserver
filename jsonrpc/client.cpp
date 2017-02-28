/*
 * client.cpp
 *
 *  Created on: 4. 5. 2016
 *      Author: ondra
 */

#include "client.h"

#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/base/actions/promise.tcc"

#include "errors.h"
namespace jsonrpc {

Client::Client(const ClientConfig& cfg)
	:http(cfg),jsonFactory(cfg.jsonFactory),batchCounter(0),url(cfg.url)
{
	serialized.setStaticObj();

}

Future<Client::Result> Client::callAsync(StrViewA method, JValue params, JValue context) {
	//lock batches, to avoid conflicts during access
	Synchronized<FastLock> _(batchAccess);

	//build request
	JObject obj;
	obj("id", preparedList.length())
			("method",method)
			("params",params);
	//if context is set, add to request
	if (context.defined()) {
		obj("context",context);
	}

	//add request to prepare list, create promise
	Future<Client::Result> res;
	preparedList.add(PreparedItem(obj,res.getPromise()));

	//if no batch active...
	if (batchCounter == 0) {
		//open single request batch
		batchCounter = 1;
		//unlock batch access
		SyncReleased<FastLock> _(batchAccess);
		//run this batch now
		runBatch();
	}

	//in all cases, return future
	return res;
}

Client::Result Client::call(StrViewA method, JValue params, JValue context) {
	return callAsync(method,params,context).getValue();
}

void Client::beginBatch() {
	Synchronized<FastLock> _(batchAccess);
	//just increase batch counter
	batchCounter++;

}

void Client::closeBatch() {
	Synchronized<FastLock> _(batchAccess);
	//if any batch active
	if (batchCounter > 0) {
		//this is last closeBatch?
		if (batchCounter == 1) {
			//yes, close batch now (there should not be other closeBatch, so we can release the lock)
			SyncReleased<FastLock> _(batchAccess);
			//now run batch
			//the batch is still seen opened, so we can collect other requests
			runBatch();
		} else {
			//just decrease batch counter
			batchCounter --;
		}
	}
}

void Client::runBatch() {
	using namespace BredyHttpClient;
	//lock processing (wait until other request processes)
	//we are still in opened batch - other requests can be collected
	Synchronized<FastLock> _(processingAccess);
	//there will be list of processing requests
	PreparedList processing;
	//clear serialized request from previous run
	serialized.clear();
	try {
		{
			//lock batch access - now closing collector
			Synchronized<FastLock> __(batchAccess);
			//release final count of batches
			//there can be different number than 1. Someone other could have opened batch.
			batchCounter--;
			//if prepared list is empty (how?) quit now
			if (preparedList.empty()) return;
			//move prepared list to processing
			processing.swap(preparedList);

		}
		http.open(HttpClient::mPOST,url);
		http.setHeader(HttpClient::fldContentType,"application/json");
		http.setHeader(HttpClient::fldAccept,"application/json");
		SeqFileOutput body = http.beginBody();


		//build request to memory
		for (PreparedList::Iterator iter = processing.getFwIter(); iter.hasItems();) {
			iter.getNext().request.serialize([&](char c){body.write(c);});
		}

		SeqFileInput response = http.send();

		//check status (should be 200)
		if (http.getStatus() == 200) {

			//process all requests
			for (natural i = 0; i < processing.length(); i++) {
				JValue r;
				//parse response
				{
					Synchronized<FastLock> _(batchAccess);
					r = JValue::parse([&](){return response.getNext();});
				}
				//pick "id"
				JValue vid = r["id"];
				//id is equal to index
				//if id is not defined, this is strange situation, ignore response
				if (vid!= nil) {
					//pick id
					natural id = vid.getUInt();
					//check whether id in range
					if (id < processing.length()) {
						//pick request
						const PreparedItem &itm = processing[id];
						//it should not be done
						if (!itm.done) {
							//mark it done
							itm.done = true;
							//pick result
							JValue result = r["result"];
							//pick error
							JValue error = r["error"];
							//pick context
							JValue context = r["context"];
							//if error
							if (error.defined() && !error.isNull()) {
								//reject promise
								itm.result.reject(RemoteException(THISLOCATION,error));
							} else {
								//otherwise resolve promise
								itm.result.resolve(Result(result,context));
							}
						}
					}
				}
			}

			//process all requests that aren't done
			for (natural i = 0; i < processing.length(); i++) {
				if (!processing[i].done) {
					//reject them
					processing[i].result.reject(HttpStatusException(THISLOCATION,url,500,"No response from the server"));
				}
			}


		} else {
			//if other status then 200 reject all promises with HttpStatusException
			PException exp = new HttpStatusException(THISLOCATION,url,http.getStatus(),http.getStatusMessage());
			for (PreparedList::Iterator iter = processing.getFwIter(); iter.hasItems();) {
				iter.getNext().result.reject(exp);
			}
		}
		//also skip remain body
		http.close();
	} catch (const Exception &e) {
		http.close();
		//in case of exception, reject all promises with exception
		for (PreparedList::Iterator iter = processing.getFwIter(); iter.hasItems();) {
			const PreparedItem &itm = iter.getNext();
			if (!itm.done) itm.result.reject(e);
		}
	}
}

Future<Client::Result> Client::Batch::call(ConstStrA method, JValue params, JValue context) {
	return client.callAsync(method,params,context);
}

Client::PreparedItem::PreparedItem(const JValue &request,Promise<Result> result):request(request),result(result),done(false) {}
Client::PreparedItem::~PreparedItem() {}


} /* namespace jsonrpc */


template class LightSpeed::Promise<jsonrpc::Client::Result>;
