/*
 * HttpReqImpl.h
 *
 *  Created on: Sep 7, 2012
 *      Author: ondra
 */

#ifndef JSONRPCSERVER_HTTPREQIMPL_H_
#define JSONRPCSERVER_HTTPREQIMPL_H_

#include "httprequest.h"
#include "lightspeed/base/streams/fileio.h"
#include "lightspeed/base/streams/netio.h"
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/framework/ITCPServer.h"

#include "lightspeed/base/containers/stringpool.h"
#include "nstream.h"

namespace LightSpeed {
	class Semaphore;
}

namespace BredyHttpSrv {

class ConnContext;

class HttpReqImpl: public IHttpRequest{
public:

	HttpReqImpl(ConstStrA baseUrl, ConstStrA serverIdent, Semaphore &busySemaphore);


	virtual ConstStrA getMethod();
	virtual ConstStrA getPath();
	virtual ConstStrA getProtocol();
	virtual HeaderValue getHeaderField(ConstStrA field) const;
	virtual HeaderValue getHeaderField(HeaderField field) const ;
	virtual bool enumHeader(Message<bool,HeaderFieldPair > caller)const;
	virtual void sendHeaders();
	virtual void header(ConstStrA field, ConstStrA value);
	virtual void header(HeaderField field, ConstStrA value);
	virtual void status(natural code, ConstStrA msg = ConstStrA());
	virtual void errorPage(natural code, ConstStrA msg = ConstStrA(), ConstStrA expl = ConstStrA());
	virtual void redirect(ConstStrA url, int code = 0);
	virtual ConstStrA getBaseUrl() const;
	virtual void useHTTP11(bool use);
	virtual bool isField(ConstStrA text, HeaderField fld) const ;


	virtual natural read(void *buffer,  natural size);
    virtual natural write(const void *buffer,  natural size);
	virtual natural peek(void *buffer, natural size) const;
	virtual void finish();
	virtual bool canRead() const;
	virtual bool canWrite() const;
	virtual void flush();
	virtual natural dataReady() const;
	virtual void closeOutput();
	virtual bool headersSent() const {return bHeaderSent;}
	virtual natural callHandler(ConstStrA path, IHttpHandler **h);
	virtual natural callHandler(IHttpRequest &request, IHttpHandler **h);
	virtual natural callHandler(IHttpRequest &request, ConstStrA path, IHttpHandler **h)  = 0;
	virtual natural forwardRequest(ConstStrA path);
	virtual bool keepAlive() const {return !closeConn;}


	void finishChunk();


	ITCPServerConnHandler::Command onData(NStream &stream);
	ITCPServerConnHandler::Command  readHeader();
	ITCPServerConnHandler::Command  finishReadHeader();
	void clear();

	virtual void setRequestContext(IHttpHandlerContext *context) ;
	virtual void setConnectionContext(IHttpHandlerContext *context) ;
	virtual IHttpHandlerContext *getRequestContext() const ;
	virtual IHttpHandlerContext *getConnectionContext() const ;
	virtual natural getPostBodySize() const;

	virtual PNetworkStream getConnection();




	virtual void beginIO();
	virtual void endIO();

	virtual void setMaxPostSize(natural bytes) ;
	virtual void attachThread(natural status);

	ITCPServerConnHandler::Command  onUserWakeup();


protected:

	struct HttpHeaderCmpKeys {
	public:
		bool operator()(ConstStrA a, ConstStrA b) const;
	};


	typedef StringPool<char, SmallAlloc<256> > StrPool;
	typedef AutoArray<byte, SmallAlloc<256> > ChunkBuff;
	typedef StrPool::Str HdrStr;
	typedef Map<HdrStr, HdrStr, HttpHeaderCmpKeys> HeaderMap;




	void writeChunk(const void *data, natural len);
	ITCPServerConnHandler::Command  errorPageKA(natural code, ConstStrA expl = ConstStrA());
	void send100continue() const;
	ITCPServerConnHandler::Command  processHandlerResponse(natural res);
	bool isInputAvailable() const;
	

protected:
	NStream *inout;
	ConstStrA serverIdent;
	ConstStrA baseUrl;
	unsigned short httpMajVer;
	unsigned short httpMinVer;
	mutable natural remainPostData;
	bool bHeaderSent;
	bool isHeadMethod;
	bool useChunked;
	bool closeConn;
	bool outputClosed;
	bool switchedProtocol;
	mutable bool inputClosed;
	mutable bool bNeedContinue;
	mutable bool chunkedPost;

	StrPool hdrPool,responseHdrPool;
	HeaderMap requestHdrs, responseHdrs;
	natural statusCode;
	HdrStr statusMsg;
	ChunkBuff chunkBuff;

	POutputStream curOutput;

	HdrStr method,path,protocol;
	Pointer<IHttpHandler> curHandler;
	AllocPointer<IHttpHandlerContext> requestContext;
	AllocPointer<IHttpHandlerContext> connectionContext;
	natural bufferAllocPos;
	Semaphore &busySemaphore;
	natural busyLockStatus;
	mutable natural postBodyLimit;
	natural attachStatus;

	class SectionIODirect {
	public:
		SectionIODirect(HttpReqImpl &impl):impl(impl) {
			impl.HttpReqImpl::beginIO();
		}
		SectionIODirect(const HttpReqImpl &impl):impl(const_cast<HttpReqImpl &>(impl)) {
			this->impl.HttpReqImpl::beginIO();
		}
		~SectionIODirect() {
			impl.HttpReqImpl::endIO();
		}
		HttpReqImpl &impl;
	};


	class SectionIODirectRev {
		public:
			SectionIODirectRev(HttpReqImpl &impl):impl(impl) {
				impl.HttpReqImpl::endIO();
			}
			~SectionIODirectRev() {
				impl.HttpReqImpl::beginIO();
			}
			HttpReqImpl &impl;
		};




private:
	void openPostChunk() const;
};

} /* namespace jsonsvc */
#endif /* JSONRPCSERVER_HTTPREQIMPL_H_ */
