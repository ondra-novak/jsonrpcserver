/*
 * clientWSAutoCon.cpp
 *
 *  Created on: 11. 7. 2016
 *      Author: ondra
 */




#include "clientWSAutoCon.h"

#include "lightspeed/base/debug/dbglog.h"

#include "lightspeed/base/exceptions/netExceptions.h"
namespace jsonrpc {

ClientWSAutoConn::ClientWSAutoConn(const ClientConfig& cfg):Super(cfg),reconnectMsg(0),reconnectDelay(1) {
}


void ClientWSAutoConn::onConnect() {
	LS_LOG.info("(WS:%1) Established connection") << cfgurl;
	Super::onConnect();
}

void ClientWSAutoConn::scheduleReconnect() {
	natural d = reconnectDelay;
	reconnectDelay = (reconnectDelay * 3+1)/2;
	if (reconnectDelay > 60) reconnectDelay = 60;
	reconnectMsg = scheduler->schedule(
			ThreadFunction::create(this, &ClientWSAutoConn::reconnect), d);
}

void ClientWSAutoConn::onLostConnection(natural code) {
	LS_LOG.info("(WS:%1) Lost connection (code: %2)") << cfgurl << code;
	ClientWS::onLostConnection(code);
	scheduleReconnect();
}

void ClientWSAutoConn::connect(PNetworkEventListener lst, BredyHttpSrv::IJobScheduler *scheduler) {
	this->scheduler = scheduler;

	try {
		Super::connect(lst);
		this->scheduler = scheduler;
	} catch (NetworkException &e) {
		scheduleReconnect();
		LS_LOG.info("(WS:%1) Unable to connect websockets: error:%2 - will retry") << cfgurl << e.what();
	}
}

void ClientWSAutoConn::disconnect() {
	Super::disconnect();
	scheduler->cancel(reconnectMsg,false);
}

bool ClientWSAutoConn::reconnect() {
	try {
		return Super::reconnect();
	} catch (NetworkException &e) {
		scheduleReconnect();
		LS_LOG.info("(WS:%1) Unable to reconnect websockets: error:%2 - will retry") << cfgurl << e.what();
		return true;
	}
}


}
