/*
 * simpleWebSiteHandler.cpp
 *
 *  Created on: Sep 19, 2012
 *      Author: ondra
 */

#include "simpleWebSite.h"
#include "lightspeed/base/containers/arrayref.h"
#include "lightspeed/base/countof.h"
#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/base/debug/dbglog.h"


namespace BredyHttpSrv {

using namespace LightSpeed;



SimpleWebSite::SimpleWebSite(FilePath documentRoot):documentRoot(documentRoot) {

}

natural SimpleWebSite::onRequest(IHttpRequest& request,ConstStrA vpath) {

	ConstStrA query;
	natural q = vpath.find('?');
	if (q != naturalNull) {
		query = vpath.offset(q);
		vpath = vpath.head(q);
	}
	if (vpath.empty()) {
		request.redirect(StringA(request.getPath() + ConstStrA("/")+query));
		return 0;
	}
	if (vpath == ConstStrA("/")) {
		vpath = ConstStrA("/index.html");
	}
	FilePath resPath = documentRoot;
	for (ConstStrA::SplitIterator iter = vpath.split('/'); iter.hasItems();) {
		ConstStrA t = iter.getNext();
		if (t.empty() || t == ConstStrA(".")) continue;
		if (t == "..") {
			if (resPath != documentRoot) resPath = resPath / parent;
			continue;
		}

		resPath = FilePath(resPath,true)/t;
	}
	String pathName = resPath;
	IFileIOServices &svc = IFileIOServices::getIOServices();
	if (svc.canOpenFile(pathName,IFileIOServices::fileOpenRead)) {

		ConstStrW ext = resPath.getExtension();
		ConstStrA ctx;
		if (ext == ConstStrW(L"txt")) ctx = ConstStrA("text/plain;charset=utf8");
		else if (ext == ConstStrW(L"jpg")) ctx = ConstStrA("image/jpeg");
		else if (ext == ConstStrW(L"png")) ctx = ConstStrA("image/png");
		else if (ext == ConstStrW(L"gif")) ctx = ConstStrA("image/gif");
		else if (ext == ConstStrW(L"js")) ctx = ConstStrA("application/javascript");
		else if (ext == ConstStrW(L"css")) ctx = ConstStrA("text/css");
		else if (ext == ConstStrW(L"htm")) ctx = ConstStrA("text/html;charset=utf8");
		else if (ext == ConstStrW(L"html")) ctx = ConstStrA("text/html;charset=utf8");
		else ctx = ConstStrA("application/octet-stream");

		request.header(IHttpRequest::fldContentType,ctx);
		request.header(IHttpRequest::fldCacheControl,"max-age=86400");

		try {
			byte buff[4096];
			ArrayRef<byte> abuff(buff,countof(buff));
			SeqFileInput infile(pathName,0);
			SeqFileOutput oup(&request);
			oup.blockCopy(infile,abuff);
		} catch (std::exception &e) {
			request.errorPage(403,ConstStrA(),e.what());
			LS_LOG.debug("Error reading: %1 - %2") << pathName << e.what();
		}
		return 0;
	} else {
		LS_LOG.debug("File not found: %1 ") << pathName;
		return 0;
	}
}
 /* namespace BredyHttpSrv */
}
