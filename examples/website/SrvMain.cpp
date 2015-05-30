/*
 * SrvMain.cpp
 *
 *  Created on: 31.3.2015
 *      Author: ondra
 */

#include "SrvMain.h"

namespace httpexample {

//the one and only application object

static SrvMain theApp;



SrvMain::SrvMain()
{

}

natural SrvMain::onInitServer(const Args& args, SeqFileOutput serr,
		const IniConfig& config) {

	FilePath root (getAppPathname());
	String documentRoot;
	IniConfig::Section webcfg = config.openSection("website");
	webcfg.required(documentRoot,"documentRoot");

	website = new SimpleWebSite(root/documentRoot/nil);


	return 0;
}


natural SrvMain::onStartServer(BredyHttpSrv::IHttpMapper& httpMapper) {

	httpMapper.addSite("",website);

	return 0;
}



} /* namespace qrpass */

