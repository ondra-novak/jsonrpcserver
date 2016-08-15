/*
 * SrvMain.cpp
 *
 *  Created on: 31.3.2015
 *      Author: ondra
 */

#include "SrvMain.h"
#include "lightspeed/mt/thread.h"
#include "lightspeed/base/debug/dbglog.h"
#include <lightspeed/base/text/textstream.tcc>


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
		HelloWorldContext *ctx = new HelloWorldContext;
		request.setRequestContext(ctx);
		ctx->runLongOperation(&request);
		//request is detached from the thread because we will run
		//a long operation in the different thread
		//
		//once the long operation is finished, request will be attached back
		//to a thread and onData event will be called
		return stSleep;
	}
}
natural HelloWorldHandler::onData( IHttpRequest &request )
{
	return stOK;
}


void HelloWorldContext::runLongOperation( IHttpRequest *r )
{
	//run long operation in the thread
	thr.start(ThreadFunction::create(this,&HelloWorldContext::worker,r));
}

void HelloWorldContext::worker( IHttpRequest *r ) {
	
	//example of long operation
	Thread::deepSleep(5000);

	//continue in request in the current thread

	//set content type
	r->header(IHttpRequest::fldContentType,"text/html;charset=UTF-8");

	r->status(200);
	//output only when not HEAD
	//this is not handled in non-server thread
	if (r->getMethod() != "HEAD")
	{
		SeqFileOutput output(r);
		PrintTextA print(output);

		//print to output - headers are sent automatically
		print("<!DOCTYPE html>");
		print("<html><head><title>Hello, World!</title></head>");
		print("<body><h1>Hello, World!</h1></body>");
		print("</html>");

		//we cannot return value, because we are not in server thread.
		//return stOK;

	}

	//onData will be called, which returns stOK
	r->wakeUp();
	//request is finished
	//(at this point, r is no longer valid)


}


HelloWorldContext::~HelloWorldContext()
{
	//destructor is called in different thread. Ensure that thread is already finished
	thr.join();
}

} /* namespace qrpass */

