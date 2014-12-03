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



namespace BredyHttpSrv {


ConnHandler::Command ConnHandler::onDataReady(const PNetworkStream &stream, ITCPServerContext *context) throw() {
	try {
		ConnContext *ctx = static_cast<ConnContext *>(context);
		char buff[100];
		LightSpeed::_intr::numberToString((natural)ctx,buff,100,36);
		DbgLog::setThreadName(buff,false);

		NStream str(stream);
		bool r = ctx->onData(str);
		while (r && str.dataReady()) {
			if (str.hasItems()) r = ctx->onData(str);
			else r = false;
		}
		return r?cmdWaitRead:cmdRemove;
	} catch (std::exception &e) {
		LogObject(THISLOCATION).note("Uncaught exception: %1") << e.what();
		return cmdRemove;
	}

}
ConnHandler::Command ConnHandler::onWriteReady(const PNetworkStream &, ITCPServerContext *context) throw() {
	return cmdRemove;
}
ConnHandler::Command ConnHandler::onTimeout(const PNetworkStream &, ITCPServerContext *context) throw () {
	return cmdRemove;
}
void ConnHandler::onDisconnectByPeer(ITCPServerContext *) throw () {}

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
		ConstStrA vpath = request.getPath().offset(rc.key.length());
		natural res = rc.value->onRequest(request,vpath);
		if (request.headersSent() || res != 0) {
			*h = rc.value;
			return res;
		}
	}
	*h = 0;
	return 404;
}



ConnContext::ConnContext(ConnHandler &owner, const NetworkAddress &addr)
	:HttpReqImpl(owner.baseUrl,owner.serverIdent, owner.busySemaphore), owner(owner) {
	peerAddr = addr;
	peerAddrStr = addr.asString(false);
	(LogObject(THISLOCATION).progress("New connection %1 - address: %2 ") << setBase(36)
			<< (natural)this << peerAddrStr) << setBase(10);
}

ConnContext::~ConnContext() {
	(LogObject(THISLOCATION).progress("Connection closed %1") << setBase(36) << (natural)this) << setBase(10);

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
ConstStrA ConnContext::getPeerAddrStr() const {
	return peerAddrStr;
}


natural ConnContext::getSourceId() const {
	return controlObject->getSourceId();
}

void ConnContext::setControlObject(Pointer<ITCPServerConnControl> controlObject) {
	 this->controlObject = controlObject;
}
}
