/*
 * HttpStream.h
 *
 *  Created on: Jan 20, 2016
 *      Author: ondra
 */

#ifndef BREDY_HTTPCLIENT_HTTPSTREAM_H_2094e820934809ei202
#define BREDY_HTTPCLIENT_HTTPSTREAM_H_2094e820934809ei202

#include <lightspeed/base/streams/fileio_ifc.h>
#include <lightspeed/base/text/textstream.h>

#include "../httpserver/headerValue.h"
#include "lightspeed/base/containers/stringpool.h"

namespace BredyHttpClient {

using namespace LightSpeed;

///Keeps state of single request
class HttpRequest: public IOutputStream, public DynObject, public BredyHttpSrv::HeaderFieldDef {
public:
	HttpRequest(IOutputStream *com, ConstStrA path, ConstStrA method, bool useHTTP11 = true);
	HttpRequest(IOutputStream *com, ConstStrA path, Method method, bool useHTTP11 = true);
	virtual ~HttpRequest();

	///returns true whether headers has been already sent
	bool headersSent() const;


    virtual natural write(const void *buffer,  natural size);
	virtual bool canWrite() const;
	virtual void flush();
	virtual void closeOutput();

	///Automatically chooses best way to transfer content of unknown length
	/** For http/1.1 this switches to chunked protocol. FOr http/1.0 this equal to
	 * calculateLength. This is default
	 */
	static const natural unknownLength = naturalNull;
	///Calculates length
	/** This require to buffer content complete */
	static const natural calculateLength = naturalNull -1;

	///Turn off handling content length. Caller will handle by self
	/** You can disable handling of length and allow to caller handle it manually
	 *
	 * With this settings, you can redefine Content-Length and Transfer-Encoding
	 */
	static const natural userHandledLength = naturalNull -2;

	///Sets expected length of content
	/** You have to call this function before the first byte is written.
	 *
	 * @param length length of the content. Use the constant unknownLength to declare that length is not
	 * known yet. The object will use Chunked encoding for the request data in case that HTTP/1.1 is
	 * enabled. Otherwise, it will buffer whole request to calculate length. Use the constant calculateLength
	 * to enforce buffering whole request even if HTTP/1.1 is enabled
	 */
	void setContentLength(natural length);

	///sends header to the output
	/**
	 * @param hdrField header field
	 * @param value header value
	 *
	 * @note headers are sent as this method called. There is no logic to eliminate duplicated headers
	 *
	 * @note Do not set Content-Length and Transfer-Encoding unless you need to do something special.
	 * Once these headers appear, object will deactiva content encoder. See setContentLength
	 */
	void setHeader(Field hdrField, ConstStrA value);

	///sends header to the output
	/**
	 * @param hdrField header field
	 * @param value header value
	 *
	 * @note headers are sent as this method called. There is no logic to eliminate duplicated headers
	 */
	void setHeader(ConstStrA hdrField, ConstStrA value);


	///Enforces object finish headers and starts body of message without need to write anything to the stream
	void beginBody();

	natural getChunkMaxSize() const;
	void setChunkMaxSize(natural chunkMaxSize);
	natural getChunkMinSize() const;
	void setChunkMinSize(natural chunkMinSize);
	IOutputStream& getCom();
	static natural getDefaultChunkMaxSize();
	static void setDefaultChunkMaxSize(natural defaultChunkMaxSize);
	static natural getDefaultChunkMinSize();
	void setDefaultChunkMinSize(natural defaultChunkMinSize);

protected:

	enum BodyHandling {
		///use remainLength to monitor and terminate writing when needed
		useRemainLength,
		///use chunked transfer protocol
		useChunked,
		///buffer whole input and calculate length. Send body and header when output is closed
		useBuffered,
		///user defined handling, do not care about length
		useDefinedByUser
	};

	BodyHandling bodyHandling;
	natural remainLength;
	AutoArray<byte> chunk;
	bool inBody;
	bool needBody;
	bool canChunked;
	PrintTextA print;
	IOutputStream &com;

	static natural defaultChunkMinSize, defaultChunkMaxSize;
	natural chunkMinSize, chunkMaxSize;
	bool outputClosed;



	void initRequest(ConstStrA path, ConstStrA method, bool useHTTP11);

	void flushChunk();
	void writeChunk(ConstBin data);

};


class HttpResponse: public IInputStream, public DynObject, public BredyHttpSrv::HeaderFieldDef {
public:
	HttpResponse(IOutputStream &com);
	virtual ~HttpResponse();

protected:
	IInputStream &com;

	typedef StringPool<char> StrPool;
	typedef StrPool::Str Str;

	StrPool pool;
	Str statusMessage;
	natural status;


};

} /* namespace BredyHttpClient */

#endif /* BREDY_HTTPCLIENT_HTTPSTREAM_H_2094e820934809ei202 */
