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

using LightSpeed::NetworkStream;

namespace BredyHttpSrv {

using namespace LightSpeed;

class ConnContext;

class ConnHandler: public ITCPServerConnHandler, public IHttpMapper {
public:
	typedef NetworkStream<4096> NStream;

	ConnHandler(StringA baseUrl, StringA serverIdent, natural maxBusyThreads)
		:serverIdent(serverIdent),baseUrl(baseUrl),busySemaphore((atomic)maxBusyThreads) {}

	virtual Command onDataReady(const PNetworkStream &stream, ITCPServerContext *context) throw();
	virtual Command onWriteReady(const PNetworkStream &stream, ITCPServerContext *context) throw();
	virtual Command onTimeout(const PNetworkStream &stream, ITCPServerContext *context) throw ();
	virtual void onDisconnectByPeer(ITCPServerContext *context) throw ();
	virtual ITCPServerContext *onIncome(const NetworkAddress &addr) throw();
	virtual Command onAccept(ITCPServerConnControl *controlObject, ITCPServerContext *context);


	friend class ConnContext;

	void addSite(ConstStrA path, IHttpHandler *handler);
	void removeSite(ConstStrA path);

	virtual void *proxyInterface(IInterfaceRequest &p);
	virtual const void *proxyInterface(const IInterfaceRequest &p) const;

	virtual void recordRequestDuration(natural ) {}


protected:
	StringA serverIdent;
	StringA baseUrl;
	PathMapper pathMap;
	natural numThreads;
	Semaphore busySemaphore;



};

class ConnContext: public ITCPServerContext, public HttpReqImpl, public IHttpPeerInfo  {
public:
	ConnHandler &owner;
	NetworkAddress peerAddr;
	mutable StringA peerAddrStr;
	Pointer<ITCPServerConnControl> controlObject;

	ConnContext(ConnHandler &owner, const NetworkAddress &addr);
	~ConnContext();

	virtual natural callHandler(IHttpRequest &request, ConstStrA path, IHttpHandler **h);

	virtual void *proxyInterface(IInterfaceRequest &p);
	virtual const void *proxyInterface(const IInterfaceRequest &p) const;
	virtual const NetworkAddress &getPeerAddr() const {return peerAddr;}
	virtual ConstStrA getPeerAddrStr() const;
	virtual natural getSourceId() const;
	void setControlObject(Pointer<ITCPServerConnControl> controlObject);

};

} /* namespace jsonsvc */
#endif /* JSONRPCSERVER_CONNHANDLER_H_ */
