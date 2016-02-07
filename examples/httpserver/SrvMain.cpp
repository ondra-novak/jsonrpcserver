/*
 * SrvMain.cpp
 *
 *  Created on: 31.3.2015
 *      Author: ondra
 */

#include "SrvMain.h"

#include <lightspeed/base/text/textstream.tcc>

using LightSpeed::PrintTextA;
namespace httpexample {

//the one and only application object

static SrvMain theApp;



SrvMain::SrvMain()
{

}

natural SrvMain::onInitServer(const Args& args, SeqFileOutput serr,
		const IniConfig& config) {


	website = new HelloWorldHandler;

	return 0;
}


natural SrvMain::onStartServer(BredyHttpSrv::IHttpMapper& httpMapper) {

	httpMapper.addSite("",website);

	return 0;
}


natural HelloWorldHandler::onRequest( IHttpRequest &request, ConstStrA vpath )
{
	///Get method
	ConstStrA method = request.getMethod();
	///Check method, not necessary, just for example
	if (method != "GET" && method != "HEAD") {
		//post list of allowed methods
		request.header(IHttpRequest::fldAllow,"GET, HEAD");
		//send status 405 Method Not Allowed
		return stMethodNotAllowed;
	} else {
		//set content type
		request.header(IHttpRequest::fldContentType,"text/html;charset=UTF-8");
		//method HEAD don't need to be handled
		//handler is leaved on first output through the exception

		SeqFileOutput output(&request);
		PrintTextA print(output);

		//print to output - headers are sent automatically
		print("<!DOCTYPE html>");
		print("<html><head><title>Hello, World!</title></head>");
		print("<body><h1>Hello, World!</h1><p>vpath: %1<br>host: %2<br>path: %3</body>") << vpath << request.getHeaderField("Host") << request.getPath();
		print("</html>");
		
		//return value - because headers was sent, status code cannot be changed
		return stOK;
	}
}

natural HelloWorldHandler::onData( IHttpRequest &request )
{
	///not used now
	return stOK;
}

} /* namespace qrpass */

