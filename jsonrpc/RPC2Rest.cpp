/*
 * RPC2Rest.cpp
 *
 *  Created on: 14. 1. 2016
 *      Author: ondra
 */

#include "RPC2Rest.h"

#include <lightspeed/base/text/textstream.tcc>
#include <lightspeed/utils/json/jsonexception.h>
#include "lightspeed/utils/json/jsonbuilder.h"

#include "../httpserver/queryParser.h"
#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/base/containers/map.tcc"


#include "ijsonrpc.h"
namespace jsonsrv {

RPC2Rest::RPC2Rest(IJsonRpc &jsonrpc,IJsonRpcLogObject *logobject):jsonrpc(jsonrpc),logobject(logobject) {


}



void RPC2Rest::addMethod(ConstStrA methodName, ConstStrA argMapping) {
	addMethod(methodName,"GET",methodName,argMapping);
}

void RPC2Rest::addMethod(ConstStrA vpath, ConstStrA httpMethod,
		ConstStrA methodName, ConstStrA argMapping) {

	restMap.insert(Key(StringKey<StringA>(StringA(vpath)),
			StringKey<StringA>(StringA(httpMethod)))
			,Mapping(methodName,argMapping));

}

static JSON::Value createValue(JSON::PFactory &json, ConstStrA value, bool forceStr) {
	if (forceStr) return json->newValue(value);
	else return json->fromString(value);
}

static void storeToObject(JSON::PFactory &json, JSON::Value toObj,
		ConstStrA fldName, ConstStrA value, bool isArray, bool forceStr) {

	try {

		natural p = fldName.find('.');
		if (p != naturalNull) {
			ConstStrA subName = fldName.offset(p+1);
			ConstStrA thisName = fldName.head(p);
			JSON::Value subobj = json->newClass();
			toObj->add(thisName,subobj);
			storeToObject(json,subobj,subName,value,isArray,forceStr);
		} else {

			JSON::Value data;
			if (isArray) {
				data = json->array();
				for(ConstStrA::SplitIterator iter = value.split(','); iter.hasItems();) {
					data->add(createValue(json, iter.getNext(),forceStr));
				}
			} else {
				data = createValue(json, value, forceStr);
			}
			toObj->add(fldName,data);
		}
	} catch (Exception &e) {
		throw RPC2Rest::CannotParseMemberException(THISLOCATION, fldName) << e;
	}
}

static void sendError(BredyHttpSrv::IHttpRequest &r, const Exception &e) {
	r.status(400);
	r.header(BredyHttpSrv::IHttpRequest::fldContentType,"text/plain;charset=utf-8");
	SeqFileOutput out(&r);
	SeqTextOutW txtout(out);
	txtout.blockWrite(e.getMessageWithReason(),true);
}


natural RPC2Rest::onRequest(BredyHttpSrv::IHttpRequest& request,
		ConstStrA vpath) {


	try {

		RestMap::Iterator iter = restMap.seek(Key(
				StringKey<StringA>(vpath),
				StringKey<StringA>(request.getMethod()))
		);
		bool again;
		do {
			again = false;
			if (!iter.hasItems()) return 404;
			const Key &k = iter.peek().key;
			if (k.method != request.getMethod()) return 404;
			if (vpath.head(k.vpath.length()) != k.vpath) return 404;
			ConstStrA newpath = vpath.offset(k.vpath.length());
			const Mapping &m = iter.peek().value;
			if (newpath.empty()) {
				if (!m.argMapping.empty()) return 404;
				newpath = "/";
			} else if (newpath[0] != '/') {
				iter.skip();
				again = true;
			} else {

				JSON::PFactory json = JSON::create();
				JSON::Value args = json->array();

				AutoArray<ConstStrA> pathArgs;
				BredyHttpSrv::QueryParser parser(newpath);
				newpath = parser.getPath().offset(1);

				for(ConstStrA::SplitIterator iter = newpath.split('/'); iter.hasItems();)
					pathArgs.add(iter.getNext());

				JSON::Value queryObj = json->object();
				while (parser.hasItems()) {
					const BredyHttpSrv::QueryField &fld = parser.getNext();
					ConstStrA field = fld.name;
					bool isarray = false;
					bool forceString = false;
					if (field.tail(2) == ConstStrA("[]")) {
						isarray = true;
						field = field.crop(0,2);
					}
					if (field.tail(1) == ConstStrA("$")) {
						forceString = true;
						field = field.crop(0,1);
					}
					storeToObject(json, queryObj, field, fld.value, isarray, forceString);
				}

				JSON::Value bodyObject = json->newNullNode();

				if (request.getMethod() == "POST" || request.getMethod() == "PUT") {
					SeqFileInput in(&request);
					bodyObject = json->fromStream(in);
				}

				JSON::Value context = json->newNullNode();
				HeaderValue hv = request.getHeaderField("X-Context");
				if (hv.defined) {
					StringA fullCtx = ConstStrA("{")+hv+ConstStrA("}");
					context = json->fromString(fullCtx);
				}


				JSON::Value id = json->newValue(request.getIfc<BredyHttpSrv::IHttpPeerInfo>().getPeerRealAddr());
				natural argpos = 0;
				for(StringA::Iterator iter = m.argMapping.getFwIter(); iter.hasItems();) {
					char mp = iter.getNext();
					switch(mp) {
					case 'b':args->add(bodyObject);break;
					case 'q':args->add(queryObj);break;
					default: {
						if (argpos >= pathArgs.length()) {
							continue;
						}
						ConstStrA a = pathArgs[argpos];
						switch(mp) {
						case 's': args->add(json->newValue(a));break;
						case 'n': args->add(json->fromString(a));break;
						default: return 404;
						}
						argpos++;
						break;
					}
					}
				}

				request.header(BredyHttpSrv::IHttpRequest::fldContentType,"application/json");

				IJsonRpc::CallResult res = jsonrpc.callMethod(&request,m.methodName,args,context,id);

				if (logobject)
					logobject->logMethod(request, m.methodName, args, context, res.logOutput);


				if (!res.error->isNull()) {
					JSON::Value status = res.error->getPtr("status");
					if (status != nil) {
						ConstStrA msg;
						JSON::Value stmsg = res.error->getPtr("statusMessage");
						if (stmsg != nil) msg = stmsg->getStringUtf8();
						request.status(status->getUInt(), msg);
					}

					SeqFileOutput out(&request);
					json->toStream(*res.error,out);
				} else {
					if (res.newContext != nil) {
						ConstStrA ctx = json->toString(*res.newContext);
						ctx = ctx.crop(1,1);
						request.header("X-Context", ctx);
					}

					if (res.result->isBool() && res.result->getBool()) {
						request.status(stCreated);
						request.sendHeaders();
					} else {
						SeqFileOutput out(&request);
						json->toStream(*res.result,out);
					}


				}
			}
		}
		while (again);

	} catch (CannotParseMemberException &e) {
		sendError(request,e);
	} catch (JSON::ParseError_t &e) {
		sendError(request,e);
	}
	return 0;


}

bool RPC2Rest::CmpKeys::operator ()(const Key& a, const Key& b) const {
	StrCmpCI<char> cmp;
	CompareResult res = cmp(a.method, b.method);
	if (res == cmpResultEqual) {
		return cmp(a.vpath,b.vpath) != cmpResultLess;
	} else {
		return res != cmpResultLess;
	}

}

const char *RPC2Rest::cantParseMember= "Cannot parse member: %1";
} /* namespace jsonsrv */
