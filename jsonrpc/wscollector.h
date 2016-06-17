/*
 * wscollector.h
 *
 *  Created on: 17. 6. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_WSCOLLECTOR_H_21060465103510321303564
#define JSONRPCSERVER_WSCOLLECTOR_H_21060465103510321303564

#include "lightspeed/base/actions/promise.h"
#include "lightspeed/mt/microlock.h"

#include "lightspeed/base/containers/map.h"
#include "rpchandler.h"
#include "rpcnotify.h"


namespace jsonsrv {
using namespace LightSpeed;


///Collects jsonrpc-websocket connections
/** It allows to collect anything that supports IRpcNotify interface. It also tracks
 * live of the connections and can automatically remove the closed connections.
 *
 * You can use this object to collect single or multiple connections. You can write handler
 * which is called when connection is closed.
 *
 * Object also ensures, that code will not use already closed or destroyed connection.
 *
 * @tparam class that holds user defined data along with the connection
 *
 */
template<typename UserData>
class WsCollector {
public:

	///Sign in websocket connection
	/**
	 * @param ntf notify interface
	 * @param userData user defined data
	 */
	void signIn(IRpcNotify *ntf, const UserData &userData);
	///Sign in websocket connection
	/**
	 * @param req RpcRequest object available during method call.
	 * @param userData user defined data
	 * @retval true request supports IRpcNotify
	 * @retval false request doesn't support IRpcNotify
	 */
	bool signIn(RpcRequest *req, const UserData &userData);

	///Sign off websocket connection
	/**
	 * Removes connection from the pool
	 * @param ntf reference to connection
	 */
	void signOff(IRpcNotify *ntf);

	///Sign off websocket connection
	/** Removes connection from the pool
	 *
	 * @param req reference to websocket connection
	 * @retval true request supports IRpcNotify
	 * @retval false request doesn't support IRpcNotify
	 */
	bool signOff(RpcRequest *req);

	///Determines whether connection is member of collector
	/**
	 * @param ntf pointer to IRpcNotify interface
	 * @retval true is member
	 * @retval false not member
	 *
	 * @note Result can be inaccurate if called outside of method handler, because
	 * connection can closed immediately after function returns the result.
	 */
	bool isMember(IRpcNotify *ntf) const;
	///Determines whether connection is member of collector
	/**
	 * @param req pointer to RpcRequest interface
	 * @retval true is member
	 * @retval false not member
	 *
     * @note Result can be inaccurate if called outside of method handler, because
	 * connection can closed immediately after function returns the result.
	 *
	 */
	bool isMember(RpcRequest *req) const;

	///Performs some operation above all connections
	/**
	 * This function can be used to execute some operation above all connection.
	 *
	 * @param fn Function which is called for every live connection. Function have to
	 *  accept two arguments (IRpcNotify *, UserData) - similar to function.signIn()
	 *
	 *  @note for each call, internal collector's lock is held. Do not access collector
	 *  inside of the function otherwise a deadlock happen.
	 *
	 *  @note Sending a notification through sendNotification() may cause an exception, because
	 *  connection can be shut down. You should handle this exception otherwise it will
	 *  be thrown out.
	 *
	 */
	template<typename Fn>
	void forEach(const Fn &fn) const;

	///Prepares a notification and broadcast the message to all or some connections
	/**
	 * Function prepares a notification and then calls the function for every live connection.
	 *
	 *
	 * @param fn Function which is called for every live connection.
	 * The function have to accept three arguments (IRpcNotify *, UserData, const PreparedNotify *).
	 * The function should filter connection and send the prepared notification to all connections
	 *
	 * @param method name of method (notification)
	 *
	 * @param params arguments of method
	 *
	 *  @note for each call, internal collector's lock is held. Do not access collector
	 *  inside of the function otherwise a deadlock happen.
	 *
	 *  @note Sending a notification through sendNotification() may cause an exception, because
	 *  connection can be shut down. You should handle this exception otherwise it will
	 *  be thrown out.
	 */
	template<typename Fn>
	void forEach(const Fn &fn, ConstStrA method, JSON::ConstValue params) const;

	///Picks one connections, locks the collector and calls the function
	/**
	 *
	 * @param ntf notification to lock
	 * @param fn function to call. Function have to accept two arguments
	 * (IRpcNotify *, UserData) - similar to function.signIn()
	 * @retval true function called
	 * @retval false function cannot be called
	 *
	 * @note this is the safe method how to send single notification to the connection
	 * referenced by IRpcNotify. If the pointer contains invalid address, function
	 * rejects the request and return false. While thread runs the function,
	 * connection is locked and cannot disappear. However it is still possible
	 * that connection will be shut down, so sending notification can throw
	 * an exception.
	 */

	template<typename Fn>
	bool lock(const IRpcNotify *ntf, const Fn &fn) const;

	///clears the collector
	void clear();

	///tests whether collector is empty
	bool empty() const;

	///Called when single connection is being closed
	/**
	 * @param c reference to connection
	 * @param u user data.
	 *
	 * @note internal lock is kept during this function. Note that function is called
	 * only if connection is closed from outside. Function is not called
	 * if connection is removed by signOff()
	 *
	 */
	virtual void onClose(IRpcNotify *c, const UserData &u) {(void)c;(void)u;}
protected:

	mutable MicroLock _lock;

	class Reg {
	public:
		FutureAutoCancel<void> future;
		UserData userData;
		Reg(const FutureAutoCancel<void> &future,const UserData &userData);
	};

	typedef Map<IRpcNotify *, Reg> ConnMap;

	ConnMap connMap;

	class RemoveConnection {
	public:
		RemoveConnection(WsCollector &owner, IRpcNotify *ref):owner(owner),ref(ref) {}
		RemoveConnection(const RemoveConnection &other):owner(other.owner),ref(other.ref) {}
		void operator()() const{
			owner.removeConnection(ref,true);
		}
		void operator()(const PException &) const{
			owner.removeConnection(ref,false);
		}

		WsCollector &owner;
		IRpcNotify *ref;
	};

	void removeConnection(IRpcNotify *p, bool notify);

};


} /* namespace snapytap */

#endif /* JSONRPCSERVER_WSCOLLECTOR_H_21060465103510321303564 */
