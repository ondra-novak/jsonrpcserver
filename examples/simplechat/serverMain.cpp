/*
 * serverMain.cpp
 *
 *  Created on: 3.5.2014
 *      Author: ondra
 */

#include "serverMain.h"
#include "lightspeed/utils/FilePath.h"
#include "lightspeed/base/debug/dbglog.h"

using LightSpeed::FilePath;

namespace simplechat {

ServerMain theApp;

ServerMain::ServerMain() {
}

ServerMain::~ServerMain() {
}

natural ServerMain::onInitServer(const Args& args, SeqFileOutput serr, const IniConfig& config) {

	ConstStrA docroot;

	IniConfig::Section websec = config.openSection("web");
	websec.required(docroot,"documentRoot");

	web = new SimpleWebSite(appPath/docroot/nil);
	ws = new WebSocketHandler(this);

	return 0;
}

natural ServerMain::onStartServer(IHttpMapper& httpMapper) {

	httpMapper.addSite("",web);
	httpMapper.addSite("/ws",ws);

	return 0;
}

void ServerMain::disconnect(Conn* conn) {
	Synchronized<FastLock> _(lk);
	connSet.erase(conn);
	LS_LOG.progress("WebSockets disconnected %1") << (natural)conn;
}

void ServerMain::connect(Conn* conn) {
	Synchronized<FastLock> _(lk);
	connSet.insert(conn);
	LS_LOG.progress("WebSockets connected %1") << (natural)conn;
}

void ServerMain::broadcast(ConstStrA msg) {
	LS_LOG.progress("WebSockets broadcast message");
	AutoArray<Conn *> errors;
	{
		Synchronized<FastLock> _(lk);
		for (ConnSet::Iterator iter = connSet.getFwIter(); iter.hasItems();) {
			Conn *c = iter.getNext();
			try {
				c->sendTextMessage(msg,true);
			} catch (std::exception &e) {
				LS_LOG.error("Exception %1") << e.what();
				errors.add(c);
			}
		}
	}
	for (natural i = 0; i < errors.length(); i++)
		disconnect(errors[i]);
}


void ServerMain::Conn::onConnect() {
	owner->connect(this);
}

ServerMain::Conn::~Conn() {
	owner->disconnect(this);
}

void ServerMain::Conn::onTextMessage(ConstStrA msg) {
	owner->broadcast(msg);
}

} /* namespace simplechat */
