/*
 * ConnHandler.cpp
 *
 *  Created on: Sep 2, 2012
 *      Author: ondra
 */

#include "ConnHandler.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include <string.h>
#include "lightspeed/base/text/textParser.tcc"
#include "lightspeed/base/text/textFormat.tcc"
#include "HttpReqImpl.h"
#include "lightspeed/base/debug/dbglog.h"
#include "lightspeed/base/streams/netio.tcc"



namespace BredyHttpSrv {



static atomic contextCounter = 0;

ConnHandler::Command ConnHandler::onDataReady(const PNetworkStream &stream, ITCPServerContext *context) throw() {
	try {
		Synchronized<Semaphore> _(busySemaphore);

		stream->setWaitHandler(this);

		ConnContext *ctx = static_cast<ConnContext *>(context);
		DbgLog::setThreadName(ctx->ctxName,false);

		if (ctx->nstream == nil) {
			ctx->nstream = Constructor1<NStream,PNetworkStream>(stream.getMT());
		}

		ConnHandler::Command cmd =  ctx->onData(ctx->nstream);
		while (cmd == ConnHandler::cmdWaitRead && ctx->nstream->dataReady() > 0) {
			cmd =  ctx->onData(ctx->nstream);
		}
		if (cmd == ITCPServerConnHandler::cmdWaitUserWakeup) {
			LogObject(THISLOCATION).debug("Request uses detach feature");
		}
		return cmd;
	} catch (std::exception &e) {
		LogObject(THISLOCATION).note("Uncaught exception: %1") << e.what();
		return cmdRemove;
	}

}

natural ConnHandler::wait(const INetworkResource *res, natural waitFor, natural timeout) const {
	natural k = INetworkResource::WaitHandler::wait(res,waitFor,0);
	if (k == 0 && timeout != 0) {
		SyncReleased<Semaphore> _(busySemaphore);
		k = INetworkResource::WaitHandler::wait(res,waitFor,timeout);
	}
	return k;


}


ConnHandler::Command ConnHandler::onWriteReady(const PNetworkStream &stream, ITCPServerContext *context) throw() {
	//because handler can't wait for both reading or writing, it is always known, what expected
	//so we can route onWriteReady through onDataReady
	return onDataReady(stream,context);
}
ConnHandler::Command ConnHandler::onTimeout(const PNetworkStream &, ITCPServerContext *context) throw () {
	//when timeout - remove connection
	return cmdRemove;
}
void ConnHandler::onDisconnectByPeer(ITCPServerContext *context) throw () {
	//while stream is disconnected, we need to close all contexts
	//before rest part of object becomes invalud
	//contexts can be accessed from other threads
	//so it is responsibility of they creators to use proper synchronization
	//and perform any cleanup before contexts are destroyed
	ConnContext *ctx = static_cast<ConnContext *>(context);
	ctx->prepareToDisconnect();

}

ITCPServerContext *ConnHandler::onIncome(const NetworkAddress &addr) throw() {
	DbgLog::setThreadName("server",false);
	ConnContext* x = new ConnContext(*this,addr);
	x->setStaticObj();
	return x;
}
ConnHandler::Command ConnHandler::onAccept(ITCPServerConnControl *controlObject, ITCPServerContext *context) {
	controlObject->setDataReadyTimeout(120000);
	controlObject->setWriteReadyTimeout(120000);
	ConnContext *ctx = static_cast<ConnContext *>(context);
	ctx->setControlObject(controlObject);
	return cmdWaitRead;
}







void ConnHandler::addSite(ConstStrA path, IHttpHandler* handler) {
	pathMap.addHandler(path,handler);
}

void ConnHandler::removeSite(ConstStrA path) {
	pathMap.removeHandler(path);
}

class DurationMeasure {
public:

	DurationMeasure(ConnHandler *target):target(target), begin(SysTime::now()) {}
	~DurationMeasure() {
		try {
			SysTime end = SysTime::now();
			target->recordRequestDuration((end - begin).msecs());
		} catch (...) {

		}
	}

protected:
	ConnHandler *target;
	SysTime begin;
};

natural ConnContext::callHandler(IHttpRequest &request, ConstStrA path, IHttpHandler **h) {

	DurationMeasure _(&owner);
	PathMapper::MappingIter iter = owner.pathMap.findPathMapping(path);
	while (iter.hasItems()) {
		const PathMapper::Record &rc = iter.getNext();
		ConstStrA vpath = path.offset(rc.key.length());
		natural res = rc.value->onRequest(request,vpath);
		if (request.headersSent() || res != 0) {
			if (h) *h = rc.value;
			return res;
		}
	}
	if (h) *h = 0;
	return 404;
}



ConnContext::ConnContext(ConnHandler &owner, const NetworkAddress &addr)
	:HttpReqImpl(owner.baseUrl,owner.serverIdent, owner.busySemaphore), owner(owner) {
	natural contextId = lockInc(contextCounter);
	peerAddr = addr;
	peerAddrStr = addr.asString(false);

	TextFormatBuff<char, StaticAlloc<100> > fmt;
	fmt("Http:%1") << contextId;
	ctxName = fmt.write();


	(LogObject(THISLOCATION).progress("New connection %1 - address: %2 ") 
			<< ctxName << peerAddrStr) ;
}

ConnContext::~ConnContext() {
	prepareToDisconnect();
	DbgLog::setThreadName("",true);
	LogObject(THISLOCATION).progress("Connection closed %1") << ctxName ;

}

void* ConnContext::proxyInterface(IInterfaceRequest& p) {
	if (p.getType() == TypeInfo(typeid(ITCPServerConnControl))) {
		return controlObject.get();
	}
	void *r = HttpReqImpl::proxyInterface(p);
	if (r == 0) r = owner.proxyInterface(p);
	return r;
}

const void* ConnContext::proxyInterface(const IInterfaceRequest& p) const {
	if (p.getType() == TypeInfo(typeid(ITCPServerConnControl))) {
		return controlObject.get();
	}
	const void *r = HttpReqImpl::proxyInterface(p);
	if (r == 0) r = owner.proxyInterface(p);
	return r;
}

void* ConnHandler::proxyInterface(IInterfaceRequest& p) {
	return IHttpMapper::proxyInterface(p);
}

const void* ConnHandler::proxyInterface(const IInterfaceRequest& p) const {
	return IHttpMapper::proxyInterface(p);
}

ConnHandler::Command ConnHandler::onUserWakeup( const PNetworkStream &stream, ITCPServerContext *context ) throw()
{
	try {
		Synchronized<Semaphore> _(busySemaphore);

		ConnContext *ctx = static_cast<ConnContext *>(context);
		DbgLog::setThreadName(ctx->ctxName,false);

		//we need context otherwise close connection
		if (ctx == 0) return cmdRemove;

		return ctx->onUserWakeup();
	} catch (std::exception &e) {
		LogObject(THISLOCATION).note("Uncaught exception: %1") << e.what();
		return cmdRemove;
	}

}

ConstStrA ConnContext::getPeerAddrStr() const {
	return peerAddrStr;
}


natural ConnContext::getSourceId() const {
	return controlObject->getSourceId();
}

void ConnContext::setControlObject(Pointer<ITCPServerConnControl> controlObject) {
	 this->controlObject = controlObject;
}

void ConnContext::prepareToDisconnect() {
	//destroy context, because there still can be other thread accessing it
	setRequestContext(0);
	//destroy context, because there still can be other thread accessing it
	setConnectionContext(0);

}

}
