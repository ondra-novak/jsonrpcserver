/*
 * iconnection.h
 *
 *  Created on: 4. 10. 2016
 *      Author: ondra
 */

#ifndef JSONRPC_JSONSERVER_IPEER_H_
#define JSONRPC_JSONSERVER_IPEER_H_


namespace jsonrpc {

using namespace LightSpeed;

class IRpcNotify;
class IClient;

///User defined context
/** You can associate an object with the peer, this object must inherit the Context class. To
 * convert Context to your object you need to use dynamic_cast<> operator.
 *
 * The class allows you to choose way how to detach or destroy the object from the peer in
 * situation when peer is closed, destroyed, or the context is replaced. Default implementation
 * destroys the context by calling delete operator;
 */
class Context {
public:

	virtual ~Context() {}

	///called when context is released from the ownership of IPeer object
	/** Default implementation is delete this */
	virtual void release() {delete this;}
};


class ContextRelease {
public:
	void destroyInstance(Context *p) {p->release();}
};

typedef AllocPointer<Context, ContextRelease> ContextVar;

///Access various parts of an object which represents peer connection
/** Object behind this interface can be remote client connecting through the http connection,
 * however it can be local client which communicates directly. This interface inserting
 * layer between server and remote client. It can be used to access various services which
 * can or cannot be available for specified type of peer
 *
 * Interface inherits IInterface. This allows to expose additional interfaces and provide
 * additional services which are not included in the IPeer interface
 */
class IPeer: public IInterface {
public:

	///Receives pointer to http request, which invoked current function
	/**
	 * @return pointer to IHttpRequestInfo. Function can return nullptr if there is
	 * no such object. Web-sockets interface puts there pointer to request which initiated
	 * web-socket's protocol
	 */
	virtual BredyHttpSrv::IHttpRequestInfo *getHttpRequest() const = 0;
	///Retrieves name of the peer
	/** Textual name can contain IP address of the peer. Return value is intended to be displayed or logged
	 *
	 * @return IP address, host if available, or another name for non-network peers.
	 */
	virtual ConstStrA getName() const = 0;
	///Index of port in port-table used to make this connection.
	/**
	 * @return function returns 0 if connecting arrived through the main port. Otherwise it
	 * contains index into port table which identifies port which has been used. By default,
	 * port with index 0 is considered as secured port where other ports are insecure. You
	 * can limit functions that are requested through port other than 0;
	 *
	 * @note non-network peers can return any number including zero, it depends on its configuration.
	 */
	virtual natural getPortIndex() const = 0;

	///Pointer to notify service
	/**
	 * @return Some peers can receive peer notification. Function returns pointer to
	 * interface which provides notification service. If the peer cannot receive notification,
	 * the function returns nullptr
	 */
	virtual IRpcNotify *getNotifySvc() const = 0;
	///sets context associated with the peer
	/**
	 * @param ctx pointer to context. It must be allocated using new operator. Function takes ownership
	 * of that object and will delete it once it is no longer needed.
	 *
	 * @note context is deleted once the peer is closed or when it is replaced.
	 */
	virtual void setContext(Context *ctx) = 0;
	///Retrieves current context
	virtual Context *getContext() const = 0;
	///Retrieves pointer to client interface
	/**
	 *
	 * @return some peers supports reversed RPC. This allows to server to call methods on client. The
	 * server can use IClient interface to achieve this. Note that server should not block the
	 * thread while waiting to response. The function returns nullptr if no client is available.
	 */
	virtual IClient *getClient() const = 0;


};


}



#endif /* JSONRPC_JSONSERVER_IPEER_H_ */
