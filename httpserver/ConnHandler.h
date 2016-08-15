/*
 * ConnHandler.h
 *
 *  Created on: Sep 2, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_CONNHANDLER_H_
#define JSONRPCSERVER_CONNHANDLER_H_

#include "lightspeed/base/framework/TCPServer.h"
#include "lightspeed/base/containers/autoArray.h"
#include "lightspeed/base/streams/netio.h"
#include "httprequest.h"
#include "pathMapper.h"
#include "HttpReqImpl.h"
#include "lightspeed/mt/semaphore.h"
#include "hostMapper.h"
#include "lightspeed/mt/rwlock.h"

using LightSpeed::NetworkStream;

namespace BredyHttpSrv {

using namespace LightSpeed;

class ConnContext;


class ConnHandler: public ITCPServerConnHandler, public IHttpMapper, public INetworkResource::WaitHandler {
public:

	ConnHandler( StringA serverIdent, natural maxBusyThreads)
		:serverIdent(serverIdent),busySemaphore((atomic)maxBusyThreads) {}

	virtual Command onDataReady(const PNetworkStream &stream, ITCPServerContext *context) throw();
	virtual Command onWriteReady(const PNetworkStream &stream, ITCPServerContext *context) throw();
	virtual Command onTimeout(const PNetworkStream &stream, ITCPServerContext *context) throw ();
	virtual Command onUserWakeup(const PNetworkStream &stream, ITCPServerContext *context, natural reason) throw();
	virtual void onDisconnectByPeer(ITCPServerContext *context) throw ();
	virtual ITCPServerContext *onIncome(const NetworkAddress &addr) throw();
	virtual Command onAccept(ITCPServerConnControl *controlObject, ITCPServerContext *context);

	virtual bool isPeerTrustedProxy(ConstStrA) { return false; }

	friend class ConnContext;

	void addSite(ConstStrA path, IHttpHandler *handler);
	void removeSite(ConstStrA path);
	void mapHost(ConstStrA mapLine);
	void unmapHost(ConstStrA mapLine);



	virtual void *proxyInterface(IInterfaceRequest &p);
	virtual const void *proxyInterface(const IInterfaceRequest &p) const;


	///wait handler
	virtual natural wait(const INetworkResource *res, natural waitFor, natural timeout) const;

	virtual natural callHandler(HttpReqImpl &rq, ConstStrA vpath, IHttpHandler **h);


	virtual void recordRequestDuration(natural) {}
protected:
	StringA serverIdent;	
	PathMapper pathMap;
	RWLock pathMapLock;
	HostMapper hostMap;
	natural numThreads;
	mutable Semaphore busySemaphore;



	ConstStrA getRealAddr(ConstStrA ip, ConstStrA proxies);
	StringKey<StringA> mapHost(ConstStrA host, ConstStrA vpath);
	StringA getBaseUrl(ConstStrA host);
	StringA getAbsoluteUrl(ConstStrA host, ConstStrA curPath, ConstStrA relpath);
};



class ConnContext: public ITCPServerContext, public HttpReqImpl, public IHttpPeerInfo  {
public:
	ConnHandler &owner;
	NetworkAddress peerAddr;
	mutable StringA peerAddrStr;
	mutable StringA peerRealAddrStr;
	Pointer<ITCPServerConnControl> controlObject;
	Optional<NStream> nstream;	
	StringA ctxName;
	StringKey<StringA> storedVPath;
	mutable StringA storedBaseUrl;

	ConnContext(ConnHandler &owner, const NetworkAddress &addr);
	~ConnContext();

	virtual natural callHandler(ConstStrA vpath, IHttpHandler **h);

	virtual void *proxyInterface(IInterfaceRequest &p);
	virtual const void *proxyInterface(const IInterfaceRequest &p) const;
	virtual const NetworkAddress &getPeerAddr() const {return peerAddr;}
	virtual ConstStrA getPeerAddrStr() const;
	virtual natural getSourceId() const;
	virtual bool mapHost(ConstStrA host, ConstStrA &vpath);
	virtual ConstStrA getBaseUrl() const;
	virtual StringA getAbsoluteUrl() const;
	virtual StringA getAbsoluteUrl(ConstStrA relpath) const;
	void setControlObject(Pointer<ITCPServerConnControl> controlObject);
	void prepareToDisconnect();
	virtual void clear();

	virtual ConstStrA getPeerRealAddr() const;


	virtual void recordRequestDuration(natural durationMs);
};

} /* namespace jsonsvc */
#endif /* JSONRPCSERVER_CONNHANDLER_H_ */
