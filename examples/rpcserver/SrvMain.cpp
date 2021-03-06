/*
 * SrvMain.cpp
 *
 *  Created on: 31.3.2015
 *      Author: ondra
 */

#include "SrvMain.h"
#include <lightspeed/base/containers/autoArray.tcc>
#include "jsonrpc/methodreg.tcc"


namespace httpexample {

//the one and only application object

static SrvMain theApp;



SrvMain::SrvMain()
{

}

natural SrvMain::onInitServer(const Args& args, SeqFileOutput serr,
		const IniConfig& config) {

	FilePath root (getAppPathname());
	IniConfig::Section rpccfg = config.openSection("rpc");

	rpc = new jsonrpc::Server(rpccfg);
//	rpc->init(rpccfg);
//	wsrpc = new JsonRpcWebsockets(*rpc);


	return 0;
}


natural SrvMain::onStartServer(BredyHttpSrv::IHttpMapper& httpMapper) {

	rpc->regMethod("helloWorld:",this,&SrvMain::rpcHelloWorld);
	rpc->regMethod("sumNumbers",this,&SrvMain::rpcNumberSum);
	rpc->regMethod("reverse:s",this,&SrvMain::rpcReverse);


	httpMapper.addSite("/RPC",rpc);
	//httpMapper.addSite("/wsrpc",wsrpc);

	return 0;
}



void SrvMain::onLogRotate()
{
	rpc->logRotate();
}

JSON::Value SrvMain::rpcHelloWorld( const Request &r )
{
	return r.json("Hello, World!");
}

JSON::Value SrvMain::rpcNumberSum(const Request &r )
{
	double sum = 0;
	natural count = 0;
	JSON::ConstIterator iter = r.params->getFwIter();
	while (iter.hasItems()) {
		double c = iter.getNext().getNumber();
		sum += c;
		count++;
	}
	return r.json
		("sum",sum)
		("avg",sum/count);
}

JSON::Value SrvMain::rpcReverse(const Request &r )
{
	ConstStrA txt = r.params[0].getStringA();
	return r.json(StringA(txt.reverse()));
}

} /* namespace qrpass */

