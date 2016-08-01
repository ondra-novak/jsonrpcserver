/*
 * clientWSAutoCon.h
 *
 *  Created on: 11. 7. 2016
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_JSONRPC_CLIENTWSAUTOCON_H_2087963187126785
#define JSONRPCSERVER_JSONRPC_CLIENTWSAUTOCON_H_2087963187126785
#include "../httpserver/IJobScheduler.h"
#include "clientWS.h"

namespace jsonrpc {

class ClientWSAutoConn: public ClientWS {
public:
	typedef ClientWS Super;

	ClientWSAutoConn(const ClientConfig &cfg);

	virtual void onConnect();
	virtual void onLostConnection(natural code);

	void connect(PNetworkEventListener lst, BredyHttpSrv::IJobScheduler *scheduler);
	void disconnect();
	bool reconnect();

protected:
	void scheduleReconnect();
	BredyHttpSrv::IJobScheduler *scheduler;
	void *reconnectMsg;
	natural reconnectDelay;



};

}



#endif /* JSONRPCSERVER_JSONRPC_CLIENTWSAUTOCON_H_2087963187126785 */
