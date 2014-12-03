/*
 * simpleHttpClient.h
 *
 *  Created on: Nov 21, 2012
 *      Author: ondra
 */

#ifndef SIMPLEHTTPCLIENT_H_
#define SIMPLEHTTPCLIENT_H_
#include "lightspeed/base/streams/fileio_ifc.h"
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/containers/autoArray.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/streams/netio_ifc.h"
#include "lightspeed/base/streams/netio.h"
#include "lightspeed/base/containers/optional.h"


namespace jsonrpc {

using namespace LightSpeed;


class SimpleHttpClient: public IHTTPStream {
public:
	SimpleHttpClient();


	virtual void setUserAgent(const String &uagent) ;
	virtual String getUserAgent() const ;
	virtual void setProxy(ProxyMode md, String addr, natural port) ;
	virtual ProxyMode getProxy() const ;
	virtual void getProxySettings(String &addr, natural &port) const;

	virtual void enableCookies(bool enable);
	virtual bool areCookiesEnabled() const;
	virtual natural getIOTimeout() const;
	virtual void setIOTimeout(natural timeout);

	virtual IHTTPStream &setMethod(ConstStrA method);
	virtual IHTTPStream &setHeader(ConstStrA headerName, ConstStrA headerValue);
	virtual IHTTPStream &cancel();
	virtual StringA getReplyHeaders();
	virtual ConstStrA getHeader(ConstStrA field);
	virtual bool enumHeaders(IEnumHeaders &enumHdr);
	virtual bool inConnected() const;
	virtual IHTTPStream &disableRedirect();
	virtual IHTTPStream &disableException();
	virtual IHTTPStream &connect();
	virtual natural getStatusCode();

    virtual natural read(void *buffer,  natural size);
    virtual natural write(const void *buffer,  natural size);
	virtual natural peek(void *buffer, natural size) const;
	virtual bool canRead() const;
	virtual bool canWrite() const;
	virtual void flush();
	virtual natural dataReady() const;
	void sendRequest();

	///sets url - must be called before data can be written or read
	void setUrl(StringA url);

	static ConstStrA getHost(ConstStrA url);
	static ConstStrA getPath(ConstStrA url);


protected:
	typedef AutoArray<char> HdrBuff;
	typedef Map<ConstStrA, ConstStrA> ReceivedHdrs;
	typedef StringPool<char> OutHdrBuff;
	typedef Map<StringPoolStrRef<char>, StringPoolStrRef<char> > SendHdrs;
	typedef AutoArrayStream<byte> DataBuff;
	typedef Optional<NetworkStream<> > PStream;

	enum CurMode {
			mdIdle,
			mdReading,
		};

	enum ReadMode {
		rmUnlimited,
		rmSizeLimited,
		rmChunked
	};

	String userAgent;
	ProxyMode proxyMode;
	String proxyAddr;
	natural proxyPort;
	bool noredirs;
	bool noexcepts;
	bool useHttp10;

	CurMode curMode;
	ReadMode readMode;
	natural fragmentSize;
	DataBuff dataBuff;
	HdrBuff inHdrBuff;
	OutHdrBuff outHdrBuff;
	ReceivedHdrs receivedHdrs;
	SendHdrs sendHdrs;
	StringA method;
	natural statusCode;

	StringA url;
	bool resetHeaders;
	bool firstRequest;


	PStream conn;
	StringA connAdr;
	ConstStrA curHost;
	ConstStrA curPath;
	natural timeout;
	bool proxyUsed;


private:
	bool extractChunkSize();
	void reportReadException();
	void skipBody();
	NetworkAddress getTargetAddress(ConstStrA &path);;
	void reloadConnection();
	void parseHeaders();


};

} /* namespace jsonrpc */
#endif /* SIMPLEHTTPCLIENT_H_ */
