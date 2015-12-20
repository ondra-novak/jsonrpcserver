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



SimpleWebSite::SimpleWebSite(FilePath documentRoot, natural cacheTime) :documentRoot(documentRoot)
	,cacheStr(cacheTime==0?StringA():StringA(ConstStrA("max-age=")+ToString<natural>(cacheTime))) {

}

natural SimpleWebSite::onRequest(IHttpRequest& request,ConstStrA vpath) {

	ConstStrA query;
	ConstStrA uri;
	natural q = vpath.find('?');
	if (q != naturalNull) {
		query = vpath.offset(q);
		uri = vpath.head(q);
	} else {
		uri = vpath;
	}
	if (uri.empty()) {
		request.redirect("+/");
		return 0;
	}
	if (uri == ConstStrA("/")) {
		uri = ConstStrA("/index.html");
	}
	FilePath resPath = documentRoot;
	for (ConstStrA::SplitIterator iter = uri.split('/'); iter.hasItems();) {
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
		if (ext == ConstStrW(L"txt")) ctx = ConstStrA("text/plain;charset=UTF-8");
		else if (ext == ConstStrW(L"jpg")) ctx = ConstStrA("image/jpeg");
		else if (ext == ConstStrW(L"png")) ctx = ConstStrA("image/png");
		else if (ext == ConstStrW(L"gif")) ctx = ConstStrA("image/gif");
		else if (ext == ConstStrW(L"js")) ctx = ConstStrA("application/javascript");
		else if (ext == ConstStrW(L"css")) ctx = ConstStrA("text/css");
		else if (ext == ConstStrW(L"htm")) ctx = ConstStrA("text/html;charset=UTF-8");
		else if (ext == ConstStrW(L"html")) ctx = ConstStrA("text/html;charset=UTF-8");
		else ctx = ConstStrA("application/octet-stream");

		return serverFile(request, pathName, ctx, vpath);
	} else {
		LS_LOG.debug("File not found: %1 ") << pathName;
		return 0;
	}
}

natural SimpleWebSite::serverFile(IHttpRequest& request, ConstStrW pathName, ConstStrA contentType, ConstStrA) {

	IFileIOServices &svc = IFileIOServices::getIOServices();
	try {

		PFolderIterator finfo = svc.getFileInfo(pathName);
		ToString<lnatural> etag(finfo->getModifiedTime().asUnix(), 36);

		HeaderValue v = request.getHeaderField(IHttpRequest::fldIfNoneMatch);
		if (v.defined && v == etag) return stNotModified;

		request.header(IHttpRequest::fldContentLength, ToString<lnatural>(finfo->getSize()));
		request.header(IHttpRequest::fldETag, etag);
		request.header(IHttpRequest::fldContentType,contentType);
		if (!cacheStr.empty()) request.header(IHttpRequest::fldCacheControl, cacheStr);

		byte buff[4096];
		ArrayRef<byte> abuff(buff,countof(buff));
		SeqFileInput infile(pathName,0);
		SeqFileOutput oup(&request);
		oup.blockCopy(infile,abuff);
	} catch (std::exception &e) {
		request.errorPage(404,ConstStrA(),e.what());
		LS_LOG.debug("Error reading: %1 - %2") << pathName << e.what();
	}
	return 0;
}


/* namespace BredyHttpSrv */
}
