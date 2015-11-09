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
	Synchronized<Semaphore> _(busySemaphore);
	ConnContext *ctx = static_cast<ConnContext *>(context);
		try {

		stream->setWaitHandler(this);

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
		if (cmd == cmdRemove) ctx->nstream = nil;
		return cmd;
	} catch (std::exception &e) {
		LogObject(THISLOCATION).note("Uncaught exception: %1") << e.what();
		ctx->nstream->getBuffer().discardOutput(naturalNull);
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
ConnHandler::Command ConnHandler::onTimeout(const PNetworkStream &, ITCPServerContext *) throw () {
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
	Synchronized<RWLock::WriteLock> _(pathMapLock);
	pathMap.addHandler(path,handler);
}

void ConnHandler::removeSite(ConstStrA path) {
	Synchronized<RWLock::WriteLock> _(pathMapLock);
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

natural ConnContext::callHandler(ConstStrA vpath, IHttpHandler **h) {
	return owner.callHandler(*this, vpath, h);
}
natural ConnHandler::callHandler(HttpReqImpl &rq, ConstStrA vpath, IHttpHandler **h){
	
	Synchronized<RWLock::ReadLock> _(pathMapLock);
	DurationMeasure __(this);

	PathMapper::MappingIter iter = pathMap.findPathMapping(vpath);
	while (iter.hasItems()) {
		const PathMapper::Record &rc = iter.getNext();
		ConstStrA vpathmapped = vpath.offset(rc.key.length());
		natural res = rc.value->onRequest(rq, vpathmapped);
		if (rq.headersSent() || res != 0) {
			if (h) *h = rc.value;
			return res;
		}
	}
	if (h) *h = 0;
	return 404;
}



ConnContext::ConnContext(ConnHandler &owner, const NetworkAddress &addr)
	:HttpReqImpl(owner.serverIdent, owner.busySemaphore), owner(owner) {
	natural contextId = lockInc(contextCounter);
	peerAddr = addr;

	TextFormatBuff<char, StaticAlloc<100> > fmt;
	fmt("Http:%1") << contextId;
	ctxName = fmt.write();


	(LogObject(THISLOCATION).info("New connection %1 ") << ctxName ) ;
}

ConnContext::~ConnContext() {
	prepareToDisconnect();
	DbgLog::setThreadName("",true);
	LogObject(THISLOCATION).info("Connection closed %1") << ctxName ;

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

ConnHandler::Command ConnHandler::onUserWakeup( const PNetworkStream &, ITCPServerContext *context ) throw()
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

ConstStrA trimSpaces(ConstStrA what) {
	if (what.empty()) return what;
	if (isspace(what[0])) return trimSpaces(what.crop(1,0));
	if (isspace(what[what.length() - 1])) return trimSpaces(what.crop(0,1));
	return what;
}

ConstStrA ConnHandler::getRealAddr(ConstStrA ip, ConstStrA proxies)
{
	natural sep = proxies.findLast(',');
	while (sep != naturalNull) {
		ConstStrA first = trimSpaces(proxies.offset(sep + 1));
		proxies = proxies.head(sep);
		if (first.empty()) continue;
		if (isPeerTrustedProxy(ip)) {
			ip = first;
		}
		else {
			return ip;
		}
		sep = proxies.findLast(',');
	}
	ConstStrA last = trimSpaces(proxies);
	if (isPeerTrustedProxy(ip)) {
		return last;
	}
	else {
		return ip;
	}

}

StringKey<StringA> ConnHandler::mapHost(ConstStrA host, ConstStrA vpath)
{
	Synchronized<RWLock::ReadLock> _(pathMapLock);
	return hostMap.mapRequest(host, vpath);
}

void ConnHandler::mapHost(ConstStrA mapLine)
{
	Synchronized<RWLock::WriteLock> _(pathMapLock);
	hostMap.registerUrl(mapLine);

}

StringA ConnHandler::getBaseUrl(ConstStrA host)
{
	Synchronized<RWLock::ReadLock> _(pathMapLock);
	return hostMap.getBaseUrl(host);

}

void ConnHandler::unmapHost(ConstStrA mapLine)
{
	Synchronized<RWLock::WriteLock> _(pathMapLock);
	hostMap.unregisterUrl(mapLine);
}

ConstStrA ConnContext::getPeerAddrStr() const {
	if (peerAddrStr.empty()) peerAddrStr = peerAddr.asString(false);
	return peerAddrStr;
}


natural ConnContext::getSourceId() const {
	return controlObject->getSourceId();
}

bool ConnContext::mapHost(ConstStrA host, ConstStrA &vpath)
{
	try {
		storedVPath = owner.mapHost(host, vpath);
		vpath = storedVPath;
		return true;
	}
	catch (HostMapper::NoMappingException &) {
		return false;
	}
}

ConstStrA ConnContext::getBaseUrl() const
{
	HeaderValue host = getHeaderField(fldHost);
	return storedBaseUrl = owner.getBaseUrl(host);
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


void ConnContext::clear()
{
	HttpReqImpl::clear();
	peerRealAddrStr.clear();
}

LightSpeed::ConstStrA ConnContext::getPeerRealAddr() const
{
	if (peerRealAddrStr.empty()) {
		StringA ipport = getPeerAddrStr();
		natural sep = ipport.findLast(':');
		ConstStrA ip = ipport.head(sep);
		HeaderValue proxies = getHeaderField(fldXForwardedFor);
		if (proxies.defined)
			peerRealAddrStr = owner.getRealAddr(ip, proxies);
		else
			peerRealAddrStr = ip;
	}
	return peerRealAddrStr;
}

}
