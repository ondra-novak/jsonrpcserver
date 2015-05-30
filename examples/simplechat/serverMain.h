/*
 * serverMain.h
 *
 *  Created on: 3.5.2014
 *      Author: ondra
 */

#ifndef SIMPLECHAT_SERVERMAIN_H_
#define SIMPLECHAT_SERVERMAIN_H_
#include "httpserver/serverMain.h"
#include "lightspeed/base/memory/allocPointer.h"
#include "httpserver/simpleWebSite.h"
#include "httpserver/webSockets.h"



namespace simplechat {

using namespace LightSpeed;
using namespace BredyHttpSrv;




class ServerMain: public AbstractServerMain {
public:
	ServerMain();
	virtual ~ServerMain();
	virtual natural onInitServer(const Args& args, SeqFileOutput serr, const IniConfig &config);
	virtual natural onStartServer(IHttpMapper &httpMapper);

protected:




	class Conn: public WebSocketConnection {
	public:
		Conn(IHttpRequest &request, ServerMain *owner):WebSocketConnection(request),owner(owner) {}
		~Conn();
		virtual void onConnect();
		virtual void onTextMessage(ConstStrA msg);


	protected:
		ServerMain *owner;
	};

	typedef WebSocketHandlerT<Conn, ServerMain *> WebSocketHandler;


	void connect(Conn *conn);
	void disconnect(Conn *conn);
	void broadcast(ConstStrA msg);

	typedef Set<Conn *> ConnSet;

	ConnSet connSet;
	AllocPointer<SimpleWebSite> web;
	AllocPointer<WebSocketHandler> ws;
	FastLock lk;

};

} /* namespace simplechat */
#endif /* SIMPLECHAT_SERVERMAIN_H_ */
