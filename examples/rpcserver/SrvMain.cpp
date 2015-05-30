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
	IniConfig::Section rpccfg = config.openSection("rpc");

	rpc = new JsonRpcServer;
	rpc->init(rpccfg);

	rpc->registerMethod("helloWorld:",RpcCall::create(this,&SrvMain::rpcHelloWorld));
	rpc->registerMethod("sumNumbers",RpcCall::create(this,&SrvMain::rpcNumberSum));
	rpc->registerMethod("reverse:s",RpcCall::create(this,&SrvMain::rpcReverse));

	return 0;
}


natural SrvMain::onStartServer(BredyHttpSrv::IHttpMapper& httpMapper) {

	httpMapper.addSite("/RPC",rpc);

	return 0;
}



void SrvMain::onLogRotate()
{
	rpc->logRotate();
}

JSON::PNode SrvMain::rpcHelloWorld( RpcRequest *r )
{
	return r->jsonFactory->newValue("Hello, World!");
}

JSON::PNode SrvMain::rpcNumberSum( RpcRequest *r )
{
	double sum = 0;
	natural count = 0;
	JSON::Iterator iter = r->args->getFwIter();
	while (iter.hasItems()) {
		double c = iter.getNext().node->getFloat();
		sum += c;
		count++;
	}
	return r->object()
		("sum",sum)
		("avg",sum/count);
}

JSON::PNode SrvMain::rpcReverse( RpcRequest *r )
{
	ConstStrA txt = r->argStrA(0);
	return r->jsonFactory->newValue(StringA(txt.reverse()));
}

} /* namespace qrpass */

