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

ConnHandler::ConnHandler(StringA serverIdent, natural maxBusyThreads)
	:serverIdent(serverIdent),me(this)
{
	busySemaphore = new BusyThreadsControl(maxBusyThreads);
}


ConnHandler::Command ConnHandler::onDataReady(const PNetworkStream &stream, ITCPServerContext *context) throw() {
	return processEvent(stream,context,IHttpRequest::evDataReady,0);
}

ConnHandler::Command ConnHandler::processEvent(const PNetworkStream &stream, ITCPServerContext *context, IHttpRequest::EventType event, natural reason) throw() {

	Synchronized<PBusyThreadsControl> _(busySemaphore);
	ConnContext *ctx = static_cast<ConnContext *>(context);
		try {

		stream->setWaitHandler(this);

		DbgLog::setThreadName(ctx->ctxName,false);

		if (ctx->nstream == nil) {
			ctx->nstream = Constructor1<NStream,PNetworkStream>(stream.getMT());
		}

		ConnHandler::Command cmd =  ctx->onData(ctx->nstream, event, reason);
		while (cmd == ConnHandler::cmdWaitRead && ctx->nstream->dataReady() > 0) {
			cmd =  ctx->onData(ctx->nstream, IHttpRequest::evDataReady, 0);
		}
		if (cmd == ITCPServerConnHandler::cmdWaitUserWakeup) {
			LogObject(THISLOCATION).debug("Request uses detach feature");
		}
		if (cmd == cmdRemove) ctx->nstream = nil;
		if (cmd == cmdWaitRead) {
			ctx->nstream->flush();
		}
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
		SyncReleased<PBusyThreadsControl> _(busySemaphore);
		k = INetworkResource::WaitHandler::wait(res,waitFor,timeout);
	}
	return k;


}


ConnHandler::Command ConnHandler::onWriteReady(const PNetworkStream &stream, ITCPServerContext *context) throw() {
	return processEvent(stream,context,IHttpRequest::evWriteReady,0);
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
	ConnContext* x = new ConnContext(serverIdent,me,addr,busySemaphore);
	x->addRef();
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


natural ConnContext::callHandler(ConstStrA vpath, IHttpHandler **h) {
	WeakRefPtr<ConnHandler> ownerptr(owner);
	if (ownerptr != null)
		return ownerptr->callHandler(*this, vpath, h);
	else
		return IHttpHandler::stServiceUnavailble;
}
natural ConnHandler::callHandler(HttpReqImpl &rq, ConstStrA vpath, IHttpHandler **h){
	
	Synchronized<RWLock::ReadLock> _(pathMapLock);

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



ConnContext::ConnContext(const StringA &serverIdent, const WeakRef<ConnHandler> &owner, const NetworkAddress &addr, const PBusyThreadsControl &busySemaphore)
	:HttpReqImpl(serverIdent,busySemaphore)
	,owner(owner) {
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
	if (p.getType() == TypeInfo(typeid(ITCPServerConnControl)) && controlObject != 0) {
		return controlObject.get();
	}
	void *r = HttpReqImpl::proxyInterface(p);
	if (r == 0) {
		WeakRefPtr<ConnHandler> conn(owner);
		if (conn != null) {
			r = conn->proxyInterface(p);
		}
	}
	return r;
}

const void* ConnContext::proxyInterface(const IInterfaceRequest& p) const {
	if (p.getType() == TypeInfo(typeid(ITCPServerConnControl)) && controlObject != 0) {
		return controlObject.get();
	}
	const void *r = HttpReqImpl::proxyInterface(p);
	if (r == 0) {
		WeakRefPtr<ConnHandler> conn(owner);
		if (conn != 0) {
			return conn->proxyInterface(p);
		}
	}
	return r;
}

void* ConnHandler::proxyInterface(IInterfaceRequest& p) {
	return IHttpMapper::proxyInterface(p);
}

const void* ConnHandler::proxyInterface(const IInterfaceRequest& p) const {
	return IHttpMapper::proxyInterface(p);
}

ConnHandler::Command ConnHandler::onUserWakeup( const PNetworkStream &stream, ITCPServerContext *context, natural reason) throw()
{
	return processEvent(stream,context,IHttpRequest::evUserWakeup,reason);
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

LightSpeed::StringA ConnHandler::getAbsoluteUrl(ConstStrA host, ConstStrA curPath, ConstStrA relpath)
{
	Synchronized<RWLock::ReadLock> _(pathMapLock);
	return hostMap.getAbsoluteUrl(host, curPath, relpath);

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
		WeakRefPtr<ConnHandler> conn(owner);
		if (conn == null) return false;
		storedVPath = conn->mapHost(host, vpath);
		vpath = storedVPath;
		return true;
	}
	catch (HostMapper::NoMappingException &) {
		return false;
	}
}

ConstStrA ConnContext::getBaseUrl() const
{
	WeakRefPtr<ConnHandler> conn(owner);
	if (conn == null) throwNullPointerException(THISLOCATION);
	HeaderValue host = getHeaderField(fldHost);
	return storedBaseUrl = conn->getBaseUrl(host);
}

StringA ConnContext::getAbsoluteUrl() const
{
	WeakRefPtr<ConnHandler> conn(owner);
	if (conn == null) throwNullPointerException(THISLOCATION);
	HeaderValue host = getHeaderField(fldHost);
	ConstStrA path = getPath();
	return conn->getAbsoluteUrl(host, path, ConstStrA());
}

StringA ConnContext::getAbsoluteUrl(ConstStrA relpath) const {
	WeakRefPtr<ConnHandler> conn(owner);
	if (conn == null) throwNullPointerException(THISLOCATION);
	HeaderValue host = getHeaderField(fldHost);
	ConstStrA path = getPath();
	return conn->getAbsoluteUrl(host, path, relpath);
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

void ConnContext::recordRequestDuration(natural durationMs) {
	WeakRefPtr<ConnHandler> conn(owner);
	if (conn != null) {
		HttpReqImpl::recordRequestDuration(durationMs);
		conn->recordRequestDuration(durationMs);
	}
}

LightSpeed::ConstStrA ConnContext::getPeerRealAddr() const
{
	WeakRefPtr<ConnHandler> conn(owner);
	if (peerRealAddrStr.empty()) {
		StringA ipport = getPeerAddrStr();
		natural sep = ipport.findLast(':');
		ConstStrA ip = ipport.head(sep);
		HeaderValue proxies = getHeaderField(fldXForwardedFor);
		if (proxies.defined && conn != null)
			peerRealAddrStr = conn->getRealAddr(ip, proxies);
		else
			peerRealAddrStr = ip;
	}
	return peerRealAddrStr;
}

void ConnContext::releaseOwnership() {
	Synchronized<MicroLock> _(controlObjectPtrLock);
	if (release()) delete this;
	controlObject = null;
}

ConnHandler::~ConnHandler() {
	me.setNull();
}

void ConnContext::wakeUp(natural reason) throw()
{
	{
		Synchronized<MicroLock> _(controlObjectPtrLock);
		if (controlObject != 0) {
			controlObject->getUserSleeper()->wakeUp(reason);
			return;
		}
	}
}


}

