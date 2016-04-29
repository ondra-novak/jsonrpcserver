/*
 * jsonRpc.cpp
 *
 *  Created on: Sep 22, 2012
 *      Author: ondra
 */

#include "jsonRpc.h"

#include <lightspeed/base/framework/proginstance.h>
#include <lightspeed/base/memory/staticAlloc.h>
#include "lightspeed/base/streams/fileiobuff.tcc"
#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/base/sync/tls.h"
#include "lightspeed/base/text/textstream.tcc"
#include "../httpserver/httprequest.h"
#include "../httpserver/IJobScheduler.h"
#include "lightspeed/base/containers/stringpool.tcc"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/utils/json/jsonfast.tcc"
#include "lightspeed/mt/exceptions/timeoutException.h"
#include "lightspeed/base/interface.tcc"
#include "lightspeed/base/containers/map.tcc"


#include "lightspeed/base/exceptions/httpStatusException.h"
#include "lightspeed/utils/md5iter.h"

#include "../httpserver/statBuffer.h"
#include "jsonRpcWebsockets.h"
#include "rpc.js.h"
#include "lightspeed/base/containers/map.tcc"

using LightSpeed::IInterface;
using LightSpeed::StaticAlloc;

namespace jsonsrv {



natural JsonRpc::onRequest(IHttpRequest& request, ConstStrA vpath) {
	if (!vpath.empty()) {
		if (vpath.head(9)==ConstStrA("/methods/")) {
			return dumpMethods(vpath.offset(9),request);
		} else if (vpath == ConstStrA("/client.js")) {
			return sendClientJs(request);
		} else if (vpath == ConstStrA("/ws_client.js")) {
			return sendWsClientJs(request);
		}
		return 404;

	}
	if (request.getMethod() == "GET") {
		return loadHTMLClient(request);
	} else if (request.getMethod() == "POST") {
		return initJSONRPC(request);
	} else {		
		initJSONRPC(request);
		request.header(IHttpRequest::fldAllow,"POST, GET, OPTIONS");
		request.status(request.getMethod() == "OPTIONS"?stOK:stMethodNotAllowed);
		request.sendHeaders();
		return 0;
	}
}


natural JsonRpc::loadHTMLClient(IHttpRequest& request) {
	if (clientPage.empty() || request.getIfc<IHttpPeerInfo>().getSourceId() != 0) {
		return 403;
	}
	request.header(IHttpRequest::fldContentType,"text/html;charset=UTF-8");
	try {
		SeqFileInBuff<> f(clientPage,0);
		SeqFileOutput fout(&request);
		fout.copy(f);
		return 0;
	} catch (...) {
		return stNotFound;
	}
}

static bool matchOrigin(ConstStrA origin, ConstStrA allowed) {
	StrCmpCI<char> cmp;
	for (ConstStrA::SplitIterator iter = origin.split(' '); iter.hasItems();) {
		ConstStrA originItem = iter.getNext();
		if (originItem.empty()) continue;
		for (ConstStrA::SplitIterator iter2 = allowed.split(' '); iter2.hasItems();) {
			ConstStrA allowedItem = iter2.getNext();
			if (allowedItem == "*" && originItem != "null") return true;
			if (cmp(originItem,allowedItem) == cmpResultEqual) return true;
		}
	}
	return false;
}

natural JsonRpc::initJSONRPC(IHttpRequest& request) {
	request.header(IHttpRequest::fldContentType,"application/json");
	if (corsEnabled) {
		HeaderValue hv = request.getHeaderField(IHttpRequest::fldOrigin);
		if (hv.defined) {
			if (!matchOrigin(hv, corsOrigin)) return stForbidden;
			request.header(IHttpRequest::fldAccessControlAllowMethods, "POST, GET, OPTIONS");
			request.header(IHttpRequest::fldAccessControlAllowHeaders, "Content-Type");
			request.header(IHttpRequest::fldAccessControlAllowOrigin,"*");
		}
	}
	return stContinue;
}

void JsonRpc::replyError(String msg, IHttpRequest& request, JSON::PNode idnode = nil) {
	LogObject lg(THISLOCATION);
	lg.error("RPC error: %1") << msg;
	JSON::PFactory f = getFactory();
	JSON::PNode r = f->newClass();
	r->add("result", f->newNullNode())->add("error", f->newValue(msg))->add(
			"id", idnode!= nil?idnode:f->newNullNode());
	ConstStrA out = f->toString(*r);
	request.writeAll(out.data(), out.length());
}

static bool findPragma(IHttpRequest &r) {

	using namespace LightSpeed;
	HeaderValue p = r.getHeaderField(IHttpRequest::fldPragma);
	if (p.defined != 0) {
		if (p.find(ConstStrA("json-escape-utf")) != naturalNull) return true;
	}
	return false;

}


natural JsonRpc::onData(IHttpRequest& request) {
	LS_LOGOBJ(lg);
	lg.debug("Reading JSONRPC request");
	request.setMaxPostSize(maxRequestSize);
	JSON::PFactory f = getFactory();
	JSON::IFactoryToStringProperty &prop = f->getIfc<JSON::IFactoryToStringProperty>();
	try {
		AutoArray<JSON::PNode, SmallAlloc<32> > replies;
		while (request.canRead()) {
			JSON::PNode reply = parseRequest(request,f);
			if (reply != nil) replies.add(reply);
		}
		lg.debug("Sending JSONRPC response");
		prop.enableUTFEscaping(findPragma(request));
		SeqFileOutput outdata(&request);
		for (natural i = 0; i < replies.length(); i++) {
			f->toStream(*(replies[i]),outdata);
		}
		return stOK;
	} catch (std::exception& e) {
		if (!request.headersSent()) {
			request.status(400,"Bad JSON request");
		}
		replyError(e.what(),request);
		return stOK;
	}
}


class BuildParamFormat: public IteratorBase<char, BuildParamFormat> {
public:
	BuildParamFormat(const JSON::INode *node):node(node),order(0) {
		fetchNext();
	}
	bool hasItems() const {
		return c!=0;
	}
	const char &getNext() {
		n = c;
		fetchNext();
		return n;
	}
	const char &peek() const {
		n = c;
		return n;
	}
protected:
	char c;
	mutable char n;
	const JSON::INode *node;
	natural order;

	void fetchNext() {
		if (order < node->getEntryCount()) {
			const JSON::INode *n = node->getEntry(order);
			order++;
			switch (n->getType()) {
				case JSON::ndArray: c = 'a';break;
				case JSON::ndBool: c = 'b';break;
				case JSON::ndObject: c= 'c';break;
				case JSON::ndFloat:
				case JSON::ndInt: c = 'n';break;
				case JSON::ndNull: c= 'x';break;
				case JSON::ndString: c = 's';break;
				default: c = 'u';break;
			}
		} else {
			c = 0;
		}
	}
};

JSON::PNode JsonRpc::parseRequest(IHttpRequest& request, JSON::IFactory *f) {

	LogObject lg(THISLOCATION);
	SeqFileInput indata(&request);

	while (indata.hasItems() && isspace(indata.peek())) indata.skip();


	JSON::PNode jsonreq;
	JSON::PNode newParams;
	/*
	jsonreq = f->fromStream(indata);
	*/
	SeqTextInA intext(indata);
	jsonreq = JSON::parseFast(intext,*(f->getAllocator()));

	if (jsonreq ==nil) return nil;

	if (jsonreq->getType() != JSON::ndObject) {
		replyError("Invalid rpc format",request);
		return nil;
	}

	CallResult res;
	res.id = jsonreq->getVariable("id");
	try {
		ConstStrA method = (*jsonreq)["method"].getStringUtf8();
		lg.debug("Parsing: %1") << method;		
		request.setRequestName(method);
		JSON::INode *params = jsonreq->getVariable("params");
		//no "params"
		if (params == 0) {
			//assume params empty
			newParams = f->array();
			params = newParams;

		} else if (params->getType() != JSON::ndArray) {
			//in case that params is not array, put value into array as one-item len array
			newParams = f->newArray()->add(params);
			params = newParams;
		}
		JSON::INode *context = jsonreq->getVariable("context");		
		lg.debug("Executing: %1") << method;
		res = callMethod(&request,method,params,context,res.id);
		lg.debug("Execution finished: %1") << method;
		if (logObject != nil) logObject->logMethod(request,method,params,context,res.logOutput);
	} catch (RpcCallError &e) {
		RpcError err(THISLOCATION,f,e.getStatus(),e.getStatusMessage());
		res.result = f->newNullNode();
		res.error = err.getError();
		if (!request.headersSent())
			request.status(e.getStatus(),e.getStatusMessage());
	} catch (Exception &e) {
		RpcError err(THISLOCATION,f,500,e.getMessageWithReason());
		res.result = f->newNullNode();
		res.error = err.getError();
	} catch (std::exception &e) {
		RpcError err(THISLOCATION,f,500,e.what());
		res.result = f->newNullNode();
		res.error = err.getError();
	}
	if (res.result == nil && res.error == nil) return nil;


	if (res.result == nil) res.error = f->newNullNode();
	if (res.error == nil) res.error = f->newNullNode();

	JSON::PNode fullReply = f->newClass();
	fullReply->add("id",res.id);
	fullReply->add("result",res.result);
	fullReply->add("error",res.error);
	if (res.newContext != nil) fullReply->add("context",res.newContext);
	fullReply->add("id",res.id);
	lg.debug("Request finished");
	return fullReply;

}

void JsonRpc::registerMethod(ConstStrA methodName, const IRpcCall & method,
		ConstStrA help) {

	Str m = strings(methodName);
	methodMap(m) = method.clone();
	if (!help.empty()) {
		natural q = methodName.findLast(':');
		if (q != naturalNull) methodName = methodName.head(q);
		if (methodHelp.find(methodName) == 0) {
			methodHelp.insert(strings(methodName),strings(help));
		}
	}
}

void JsonRpc::registerMethodObsolete(ConstStrA methodName) {
	Str m = strings(methodName);
	obsoleteMap.insert(m);
}


void JsonRpc::registerServerMethods(natural flags) {
	if (flags & flagEnableListMethods) {
		registerMethod("Server.listMethods:", RpcCall::create(this, &JsonRpc::rpcListMethods), ConstStrA("Lists all methods available for this server"));
	}
	if (flags & flagEnableMulticall) {
		registerMethod("Server.multicall", RpcCall::create(this, &JsonRpc::rpcMulticallN),
			ConstStrA("<p><pre>[[result,error],...] Server.multicall1 [\"methodName\",[args,...],[args,...],[args,...],...]</pre></p>"
			"<p><pre>[[result,error],...] Server.multicall [[\"methodName\",[args,...]],[\"methodName\",[args,...]],...]</pre></p>"));
	}
	if (flags & flagEnableStatHandler) {
		registerMethod("Server.stats:", RpcCall::create(this, &JsonRpc::rpcStats), ConstStrA("Show statistics - arguments are passed to the handlers"));
		registerMethod("Server.stats:c", RpcCall::create(this, &JsonRpc::rpcStats), ConstStrA("Show statistics - arguments are passed to the handlers"));
	}
	if (flags & flagDevelopMode) {
		registerMethod("Server.crash", RpcCall::create(this,&JsonRpc::rpcCrash),ConstStrA("Crashes the server - for development and testing"));
		registerMethod("Server.crashScheduler", RpcCall::create(this,&JsonRpc::rpcCrashScheduler),ConstStrA("Crashes the server in scheduler - for development and testing"));
		registerMethod("Server.help:s",RpcCall::create(this,&JsonRpc::rpcHelp), ConstStrA("Shows help page about selected method (if exists)"));
		registerMethod("Server.ping",RpcCall::create(this,&JsonRpc::rpcPing), ConstStrA("Sends back arguments"));
		registerMethod("Server.pingNotify",RpcCall::create(this,&JsonRpc::rpcPingNotify), ConstStrA("Sends back arguments as notify (requires wsrpc). If the first argument is string, it is used as name of the notify"));
	}

}

void JsonRpc::eraseMethod(ConstStrA methodName) {
	methodMap.erase(methodName);
	methodHelp.erase(methodName);
}

JSON::PFactory JsonRpc::getFactory() {
	JSON::PFactory &f = jsonFactory[ITLSTable::getInstance()];
	if (f == nil) {
		f = JSON::createFast();
	}
	return f;


}

JSON::PNode JsonRpc::rpcListMethods( RpcRequest* rq) {
	if (rq->httpRequest->getIfc<IHttpPeerInfo>().getSourceId() != 0) {
		throw RpcError(THISLOCATION,rq->jsonFactory,403,"Forbidden");
	}

	AutoArray<char> buff;
	JSON::PNode r = rq->jsonFactory->newArray();
	for (HandlerMap::Iterator iter = methodMap.getFwIter();iter.hasItems();) {
		const HandlerMap::Entity &e = iter.getNext();
		buff.clear();
		ConstStrA name = e.key;
		natural sep = name.findLast(':');
		if (sep == naturalNull) {
			r->add(rq->jsonFactory->newValue(name));
		} else {
			buff.append(name.head(sep));
			buff.append(ConstStrA(" [ "));
			bool rep = ++sep < name.length() ;
			while (rep) {
				const char *token;
				switch (name[sep]) {
				case 'a': token = "[ ]";break;
				case 'b': token = "true/false";break;
				case 'c': token = "{\"...\":  }";break;
				case 'n': token = "0";break;
				case 'x': token = "null";break;
				case 'd':
				case 's': token = "\"\"";break;
				default: token = "???";break;
				}
				buff.append(ConstStrA(token));
				rep = ++sep < name.length();
				if (rep) buff.append(ConstStrA(", "));
			}
			buff.append(ConstStrA(" ]"));
			r->add(rq->jsonFactory->newValue(ConstStrA(buff)));;
		}
	}
	return r;
}

/* namespace jsonsrv */

JSON::PNode JsonRpc::rpcHelp( RpcRequest* rq) {
	if (rq->httpRequest->getIfc<IHttpPeerInfo>().getSourceId() != 0) {
		throw RpcError(THISLOCATION,rq->jsonFactory,403,"Forbidden");
	}

	StringA n = rq->args->getEntry(0)->getStringUtf8();
	const Str *h = methodHelp.find(ConstStrA(n));
	if (h == 0) throw ErrorMessageException(THISLOCATION,ConstStrA("There is no documentation for this method: ") + n);
	return rq->jsonFactory->newValue(ConstStrA(*h));
}

static void addLine(AutoArray<char> &buffer, ConstStrA line) {
	bool openital = false, openbold = false, openunder = false;
	if (!line.empty()) {
		buffer.add(line[0]);
		for (unsigned int i = 1; i < line.length(); i++) {
			char c = line[i];
			char d = line[i-1];
			if (c == d) switch(c) {
			case '*': buffer.resize(buffer.length()-1);
					  if (!openbold) buffer.append(ConstStrA("<strong>"));
					  else buffer.append(ConstStrA("</strong>"));
					  openbold = !openbold;
					  break;
			case '/': buffer.resize(buffer.length()-1);
					  if (!openital) buffer.append(ConstStrA("<em>"));
					  else buffer.append(ConstStrA("</em>"));
					  openital = !openital;
					  break;
			case '_': buffer.resize(buffer.length()-1);
					  if (!openunder) buffer.append(ConstStrA("<u>"));
					  else buffer.append(ConstStrA("</u>"));
					  openunder = !openunder;
					  break;
			default:  buffer.add(c);
			} else {
				switch(c) {
				case '<':buffer.append(ConstStrA("&lt;"));break;
				case '>':buffer.append(ConstStrA("&gt;"));break;
				case '&':buffer.append(ConstStrA("&amp;"));break;
				default: buffer.add(c);break;
				}
			}
		}
		if (openunder) buffer.append(ConstStrA("</u>"));
		if (openital) buffer.append(ConstStrA("</em>"));
		if (openbold) buffer.append(ConstStrA("</strong>"));
	}
	buffer.append(ConstStrA("<br />"));
}

void JsonRpc::loadHelp(SeqFileInput& input) {
	ScanTextA scanner(input);
	scanner.setNL("\n");
	AutoArray<char> buffer;
	while (scanner.readLine()) {
		StringA methodName = scanner[1].str();
		if (methodName.empty()) continue;
		buffer.clear();
		buffer.append(ConstStrA("<pre>"));
		while (scanner.readLine()) {
			ConstStrA ln = scanner[1].str();
			if (ln == ".") break;
			if (!ln.empty()) {
				if (ln[0] != ' ' && ln[0] == '\t') {
					addLine(buffer,ln);
				} else {
					buffer.append(ConstStrA("<strong>"));
					addLine(buffer,ln);
					buffer.append(ConstStrA("</strong>"));
				}
			} else {
				addLine(buffer,ln);
			}
		}
		buffer.append(ConstStrA("</pre>"));
		methodHelp.insert(strings(methodName),strings(buffer));
	}

}

void JsonRpc::loadHelp(ConstStrW filename) {
	SeqFileInBuff<> f(filename,0);
	loadHelp(f);
}

JSON::PNode JsonRpc::rpcPing( RpcRequest* rq) {
	JSON::PNode nd(const_cast<JSON::INode *>(rq->args.get()));
	return nd;
}

RpcError JsonRpc::onException(JSON::IFactory *json, const std::exception &e) {
	const RpcError *rpcerror = dynamic_cast<const RpcError *>(&e);
	if (rpcerror) return *rpcerror;
	const TimeoutException *tmout = dynamic_cast<const TimeoutException *>(&e);
	if (tmout) return RpcError(THISLOCATION, json, 504,tmout->getMessageWithReason());
	const HttpStatusException *httpe= dynamic_cast<const HttpStatusException *>(&e);
	if (httpe) return RpcError(THISLOCATION, json, 5000+httpe->getStatus(),httpe->getMessageWithReason());
	const Exception *lse= dynamic_cast<const Exception *>(&e);
	if (lse) return RpcError(THISLOCATION, json, 500,lse->getMessageWithReason());
	return RpcError(THISLOCATION,json,500,e.what());

}

Optional<bool> JsonRpc::isAllowedOrigin(ConstStrA origin) {
	if (corsEnabled) {
		return Optional<bool>(matchOrigin(origin,corsOrigin));
	} else {
		return nil;
	}
}

JsonRpc::CallResult JsonRpc::callMethod(IHttpRequest *httpRequest, ConstStrA methodName, JSON::INode *args, JSON::INode *context, JSON::INode *id) {
	LogObject lg(THISLOCATION);

	typedef StringPool<char, SmallAlloc<256> > StrP;
	typedef StrP::Str Str;
	StrP pool;

	JSON::PFactory f = getFactory();

	RpcRequest rq;
	rq.args = args;
	rq.idnode = id;
	rq.context = context;
	rq.httpRequest = httpRequest;
	rq.jsonFactory = f;
	rq.serverStub = this;

	if (rq.args == 0) {
		throw RpcCallError(THISLOCATION,400,"Missing 'args' field in the RPC request header");
	}
	if (rq.idnode == 0) {
		throw RpcCallError(THISLOCATION, 400,"Missing 'id' field in the RPC request header");
	}

	Str name = pool.add(methodName);
	Str idname = pool.add(id->getStringUtf8());
	Str paramFormat = pool.add(BuildParamFormat(rq.args));
	Str exMethodName = pool.add((methodName + ConstStrA(':') + paramFormat).getFwIter());

	rq.functionName = name;
	rq.id = idname;
	rq.argFormat = paramFormat;

	JSON::PNode idnode(const_cast<JSON::INode *>(rq.idnode.get()));



	const SharedPtr<IRpcCall> *m = methodMap.find(exMethodName);
	if (m == 0) {
		m = methodMap.find(methodName);
		if (m == 0) {
			if (obsoleteMap.find(exMethodName) != 0 || obsoleteMap.find(methodName) != 0) {
				CallResult result;
				RpcError err(THISLOCATION,f,426,"Upgrade Required");
				result.result = f->newNullNode();
				result.error = err.getError();
				result.id = const_cast<JSON::INode *>(id);
				lg.error("RPC: %1 Error %2") << methodName << f->toString(*err.getError());
				if (rq.logOutput == nil) rq.logOutput = err.getError();
				return result;

			}

			throw RpcCallError(THISLOCATION,404,ConstStrA("Method not found: ") + methodName);
		}
	}

	IRpcCall &call = *(m->get());

	CallResult result;
	result.id = const_cast<JSON::INode *>(id);

	if (result.id->getType() == JSON::ndNull) {
		call(&rq); //ignore result, because this is notify
	} else {

		try {
			//call method
			result.result = call(&rq);
			//call global handlers (called only if success)
			for (HandlerMap::Iterator iter = globalHandlers.getFwIter(); iter.hasItems();) {
				IRpcCall &gcall = *(iter.getNext().value.get());
				gcall(&rq);
			}
			//set error to NULL
			result.error = f->newNullNode();
			//set new context
			result.newContext = rq.contextOut;
			//report success
			lg.info("RPC: %1 succeeded") << methodName;

		} catch (RpcCallError &) {
			throw;
		} catch (std::exception &e) {
			RpcError err = onException(f,e);
			result.result = f->newNullNode();
			result.error = err.getError();
			lg.error("RPC: %1 Error %2") << methodName << f->toString(*err.getError());
			if (rq.logOutput == nil) rq.logOutput = err.getError();
		}
		result.logOutput = rq.logOutput;
	}
	return result;
}


static void mergeContext(JSON::IFactory *f, RpcRequest *rq, JsonRpc::CallResult &callRes, JSON::PNode &saveInContext, JSON::PNode &saveOutContext) {
	if (callRes.newContext != nil) {
		if (rq->contextOut == nil) rq->contextOut = callRes.newContext;
		else rq->contextOut = saveOutContext = f->merge(*rq->contextOut,*callRes.newContext);
		if (rq->context == nil) rq->context = callRes.newContext;
		else rq->context = saveInContext = f->merge(*rq->context,*callRes.newContext);
	}
}

JSON::PNode JsonRpc::rpcMulticall1(  RpcRequest *rq) {
	const JSON::INode &args = *rq->args;
	ConstStrA methodName = args[0].getStringUtf8();
	JSON::PNode result = rq->jsonFactory->newClass();
	JSON::PNode resarr = rq->jsonFactory->newArray();
	JSON::PNode errarr = rq->jsonFactory->newArray();
	JSON::PNode saveIn, saveOut;

	for (natural i = 1; i < args.getEntryCount();i++) {
		JSON::INode *a = args.getEntry(i);
		CallResult res = callMethod(rq->httpRequest,methodName,a,rq->context,rq->idnode);
		if (res.result == nil) res.result = rq->jsonFactory->newNullNode();
		mergeContext(rq->jsonFactory,rq,res,saveIn, saveOut);
		resarr->add(res.result);
		if (res.result->isNull()) errarr->add(res.error);
	}
	result->add("results",resarr);
	result->add("errors",errarr);
	rq->logOutput = errarr;
	return result;
}

void JsonRpc::registerStatHandler(ConstStrA handlerName, const IRpcCall & method) {
	Str m = strings(handlerName);
	statHandlerMap(m) = method.clone();
}

void JsonRpc::eraseStatHandler(ConstStrA handlerName) {
	statHandlerMap.erase(handlerName);
}

JSON::PNode JsonRpc::rpcMulticallN(RpcRequest *rq) {
	const JSON::INode &args = *rq->args;
	JSON::PNode result = rq->jsonFactory->newClass();
	JSON::PNode resarr = rq->jsonFactory->newArray();
	JSON::PNode errarr = rq->jsonFactory->newArray();

	if (args.empty()) throw RpcError(THISLOCATION,rq->jsonFactory,400,"Requires arguments");
	if (args[0].getType() == JSON::ndString) return rpcMulticall1(rq);

	JSON::PNode saveIn, saveOut;


	for (natural i = 0; i < args.getEntryCount();i++) {
		const JSON::INode &fndef = args[i];
		ConstStrA methodName = fndef[0].getStringUtf8();
		JSON::INode *fnargs = fndef.getEntry(1);
		if (fnargs->getType() != JSON::ndArray)
			throw RpcError(THISLOCATION,rq->jsonFactory,400,"Server.multicall function requires arguments in following form: array of arrays of pair [\"methoName\",[args...]]");
		CallResult res = callMethod(rq->httpRequest,methodName,fnargs,rq->context,rq->idnode);
		if (res.result == nil) res.result = rq->jsonFactory->newNullNode();
		mergeContext(rq->jsonFactory,rq,res,saveIn, saveOut);
		resarr->add(res.result);
		if (res.result->isNull()) errarr->add(res.error);
	}
	result->add("results",resarr);
	result->add("errors",errarr);
	rq->logOutput = errarr;

	return result;

}

void JsonRpc::registerGlobalHandler(ConstStrA methodUID, const IRpcCall & method) {
	Str m = strings(methodUID);
	globalHandlers(m) = method.clone();
}
void JsonRpc::eraseGlobalHandler(ConstStrA methodUID) {
	globalHandlers.erase(methodUID);

}

JsonRpc::JsonRpc():maxRequestSize(4*1024*1024),corsEnabled(false),corsOrigin("*") {
}

void JsonRpc::setRequestMaxSize(natural bytes) {
	maxRequestSize = bytes;
}


JSON::PNode JsonRpc::rpcStats(RpcRequest* rq) {
	JSON::Builder json(rq->jsonFactory.get());
	JSON::Builder::Object out = json.object();

	for (HandlerMap::Iterator iter = statHandlerMap.getFwIter();iter.hasItems();) {
		const HandlerMap::Entity &e = iter.getNext();
		JSON::PNode r = e.value->operator ()(rq);
		out->add(e.key,r);
	}



	return out;
}

JSON::PNode JsonRpc::rpcPingNotify( RpcRequest *rq) {
	ConstStrA ntfName = "notify";
	JsonRpcWebsocketsConnection *conn = JsonRpcWebsocketsConnection::getConnection(*rq->httpRequest);
	if (conn == NULL) throw RpcError(THISLOCATION,rq,405,"Method requires wsRPC");
	JSON::Iterator iter = rq->args->getFwIter();
	if (iter.hasItems() && iter.peek().node->isString()) {
		ntfName = iter.getNext().node->getStringUtf8();
	}
	JSON::PNode args = rq->array();
	while (iter.hasItems()) args->add(iter.getNext().node);

	conn->sendNotification(ntfName,args);
	return rq->ok();
}


bool JsonRpc::CmpMethodPrototype::operator ()(ConstStrA a, ConstStrA b) const {
	natural l = std::min(a.length(),b.length());
	for (natural i = 0; i < l; i++) {
		if (a[i] != b[i]) return a[i] < b[i];
		if (a[i] == ':') {
			i++;
			while (i < l) {
				char ac = a[i];
				char bc = b[i];
				if (ac != bc) {
					if (ac == 'd') {
						if (bc != 's' && bc != 'n') return false;
					} else if (bc == 'd') {
						if (ac != 's' && ac != 'n') return true;
					} else return ac < bc;
				}
				i++;
			}
		}
	}
	return a.length() < b.length();
}



natural JsonRpc::dumpMethods(ConstStrA name, IHttpRequest& request) {

	if (name.tail(3) != ConstStrA(".js")) return 404;
	ConstStrA varname = name.crop(0,3);

	HeaderValue income_etag = request.getHeaderField(IHttpRequest::fldIfNoneMatch);
	if (income_etag.defined) {
		StringA chkTag = varname+methodListTag;
		if (income_etag == chkTag) return stNotModified;
	}


	JSON::PFactory fact = JSON::create();
	JSON::PNode arr = fact->array();
	ConstStrA prevMethod;
	for (HandlerMap::Iterator iter = methodMap.getFwIter(); iter.hasItems(); ) {
		const HandlerMap::Entity &e = iter.getNext();
		natural dots = e.key.find(':');
		ConstStrA mname;
		if (dots == naturalNull) mname = e.key;
		else mname = e.key.head(dots);
		if (mname != prevMethod) {
			arr->add(fact->newValue(mname));
			prevMethod = mname;
		}
	}
	ConstStrA jsonstr = fact->toString(*arr);
	HashMD5<char> hash;
	hash.blockWrite(jsonstr,true);
	hash.finish();
	StringA digest = hash.hexdigest();
	methodListTag = digest.getMT();
	StringA etag = varname+methodListTag;
	StringA result = ConstStrA("var ") + varname + ConstStrA("=") + jsonstr + ConstStrA(";\r\n");

	request.header(IHttpRequest::fldContentType,"application/javascript");
	request.header(IHttpRequest::fldContentLength,ToString<natural>(result.length()));
	request.header(IHttpRequest::fldETag,etag);
	request.writeAll(result.data(),result.length());

	return stOK;


}

natural JsonRpc::sendClientJs(IHttpRequest& request) {
	ConstBin data(reinterpret_cast<const byte *>(jsonrpcserver_rpc_js),jsonrpcserver_rpc_js_length);
	request.header(IHttpRequest::fldContentType,"application/javascript");
	request.header(IHttpRequest::fldContentLength,ToString<natural>(data.length()));
	request.header(IHttpRequest::fldCacheControl,"max-age=31556926");
	request.writeAll(data.data(),data.length());
	return stOK;
}

natural JsonRpc::sendWsClientJs(IHttpRequest& request) {
	ConstBin data(reinterpret_cast<const byte *>(jsonrpcserver_wsrpc_js),jsonrpcserver_wsrpc_js_length);
	request.header(IHttpRequest::fldContentType,"application/javascript");
	request.header(IHttpRequest::fldContentLength,ToString<natural>(data.length()));
	request.header(IHttpRequest::fldCacheControl,"max-age=31556926");
	request.writeAll(data.data(),data.length());
	return stOK;
}

JSON::PNode JsonRpc::rpcCrash(RpcRequest* rq) {
	int *x = (int *)1;
	*x = 2; //crash
	return rq->ok();
}

JSON::PNode JsonRpc::rpcCrashScheduler(RpcRequest* rq) {
	BredyHttpSrv::IJobScheduler *sch = rq->httpRequest->getIfcPtr<BredyHttpSrv::IJobScheduler>();
	sch->schedule(Action::create(this,&JsonRpc::rpcCrash,rq),3);
	return rq->ok();
}



void JsonRpc::enableCORS(bool enable) {
	corsEnabled = enable;
}

bool JsonRpc::isCORSEnabled() const {
	return corsEnabled;
}

void JsonRpc::setCORSOrigin(ConstStrA origin) {
	corsOrigin = origin;
}

ConstStrA JsonRpc::getCORSOrigin() const {
	return corsOrigin;
}

}
