/*
 * wscollector.tcc
 *
 *  Created on: 17. 6. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_WS_WSCOLLECTOR_TCC_241094633
#define JSONRPCSERVER_WS_WSCOLLECTOR_TCC_241094633

#include "lightspeed/base/actions/promise.tcc"
#include "lightspeed/base/containers/map.tcc"
#include "lightspeed/base/containers/autoArray.tcc"

#include "wscollector.h"

#include <lightspeed/base/actions/executor.h>

namespace jsonsrv {

template<typename UserData>
void WsCollector<UserData>::signIn(IRpcNotify* ntf,		const UserData& userData) {

	Synchronized<MicroLock> _(_lock);
	if (connMap.find(ntf)) return;

	Future<void> f = ntf->onClose().isolate();
	f.thenCall(RemoveConnection(*this,ntf),RemoveConnection(*this,ntf));
	connMap.insert(ntf,Reg(f,userData));
}

template<typename UserData>
bool WsCollector<UserData>::signIn(RpcRequest* req,const UserData& userData) {
	IRpcNotify *x = IRpcNotify::fromRequest(req);
	if (x == 0) return false;
	signIn(x,userData);
	return true;
}

template<typename UserData>
void WsCollector<UserData>::signOff(IRpcNotify* ntf) {
	removeConnection(ntf, false);
}

template<typename UserData>
bool WsCollector<UserData>::signOff(RpcRequest* req) {
	IRpcNotify *x = IRpcNotify::fromRequest(req);
	if (x == 0) return false;
	removeConnection(x, false);
	return true;
}

template<typename UserData>
template<typename Fn>
void WsCollector<UserData>::forEach(const Fn& fn)  {
	AutoArray<IRpcNotify *, SmallAlloc<8> > toRemove;
	{
		Synchronized<MicroLock> _(_lock);
		if (connMap.empty()) return;

		for(typename ConnMap::Iterator iter = connMap.getFwIter(); iter.hasItems();) {
			const typename ConnMap::KeyValue &kv = iter.getNext();
			try {
				fn(kv.key, kv.value.userData);
			} catch (...) {
				//we have to drop connection outside of lock, otherwise deadlock would occur
				//forEach(lock)->drop->onClose->removeConnection(lock)
				toRemove.add(kv.key);
			}
		}
	}
	for (natural i = 0, cnt = toRemove.length(); i < cnt; i++) toRemove[i]->dropConnection();
}

template<typename UserData>
WsCollector<UserData>::Reg::Reg(const FutureAutoCancel<void>& future, const UserData& userData)
	:future(future)
	,userData(userData)
{
}

template<typename UserData>
inline void WsCollector<UserData>::clear() {
	Synchronized<MicroLock> _(_lock);
	connMap.clear();

}

template<typename UserData>
inline bool WsCollector<UserData>::empty() const {
	Synchronized<MicroLock> _(_lock);
	return connMap.empty();
}

template<typename UserData>
bool WsCollector<UserData>::isMember(IRpcNotify* ntf) const {
	Synchronized<MicroLock> _(_lock);
	return connMap.find(ntf) != 0;

}

template<typename UserData>
bool WsCollector<UserData>::isMember(RpcRequest* req) const {
	IRpcNotify *x = IRpcNotify::fromRequest(req);
	if (x == 0) return false;
	return isMember(x);

}

template<typename UserData>
template<typename Fn>
void WsCollector<UserData>::forEach(const Fn& fn,
		ConstStrA method, JSON::ConstValue params) {
	AutoArray<IRpcNotify *, SmallAlloc<8> > toRemove;
	{

		Synchronized<MicroLock> _(_lock);
		if (connMap.empty()) return;


		typename ConnMap::Iterator iter = connMap.getFwIter();
		const typename ConnMap::KeyValue &kv = iter.peek();
		PreparedNotifyScope prep(kv.key, method,params);
		while (iter.hasItems()) {
			const typename ConnMap::KeyValue &kv = iter.getNext();
			try {
				fn(kv.key, kv.value.userData, prep);
			}catch (...) {
				//we have to drop connection outside of lock, otherwise deadlock would occur
				//forEach(lock)->drop->onClose->removeConnection(lock)
				toRemove.add(kv.key);
			}
		}
	}
	for (natural i = 0, cnt = toRemove.length(); i < cnt; i++) toRemove[i]->dropConnection();

}

template<typename UserData>
template<typename Fn>
void WsCollector<UserData>::forEach(const Fn& fn,
		ConstStrA method, JSON::ConstValue params, IExecutor *exec) {
	AutoArray<IRpcNotify *, SmallAlloc<8> > toRemove;
	FastLock toRemoveLock;

	class Msg: public IExecutor::IExecAction {
	public:
		LIGHTSPEED_CLONEABLECLASS;

		Msg(const Fn &fn, IRpcNotify *target, UserData &data, const PreparedNotify *prepntf,
				AutoArray<IRpcNotify *, SmallAlloc<8> > &toRemove,
				FastLock &toRemoveLock)
			:fn(fn),target(target),data(data),prepntf(prepntf),toRemove(toRemove),toRemoveLock(toRemoveLock) {}
		virtual void operator()() const {
			try {
				fn(target,data,prepntf);
			} catch (...) {
				Synchronized<FastLock> _(toRemoveLock);
				toRemove.add(target);
			}
		}
	protected:
		const Fn &fn;
		IRpcNotify *target;
		UserData &data;
		const PreparedNotify *prepntf;
		AutoArray<IRpcNotify *, SmallAlloc<8> > &toRemove;
		FastLock &toRemoveLock;
	};


	{

		Synchronized<MicroLock> _(_lock);
		if (connMap.empty()) return;


		typename ConnMap::Iterator iter = connMap.getFwIter();
		const typename ConnMap::KeyValue &kv = iter.peek();
		PreparedNotifyScope prep(kv.key, method,params);
		while (iter.hasItems()) {
			const typename ConnMap::KeyValue &kv = iter.getNext();
			exec->execute(Msg(fn,kv.key, kv.value.userData,prep,toRemove,toRemoveLock));
		}
	}
	for (natural i = 0, cnt = toRemove.length(); i < cnt; i++) toRemove[i]->dropConnection();

}

template<typename UserData>
template<typename Fn>
bool WsCollector<UserData>::lock(const IRpcNotify* ntf,
		const Fn& fn) const {
	Synchronized<MicroLock> _(_lock);
	const Reg *v = connMap.find(ntf);
	if (v == 0) return false;
	fn(ntf, v->userData);

}


template<typename UserData>
void WsCollector<UserData>::removeConnection( IRpcNotify* p, bool notify) {
	Synchronized<MicroLock> _(_lock);
	if (notify) {
		const Reg *x = connMap.find(p);
		if (x) onClose(p,x->userData);
	}
	connMap.erase(p);
}


}



#endif /* JSONRPCSERVER_WS_WSCOLLECTOR_TCC_241094633 */
