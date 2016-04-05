/*
 * HttpStream.h
 *
 *  Created on: Jan 20, 2016
 *      Author: ondra
 */

#ifndef BREDY_HTTPCLIENT_HTTPSTREAM_H_2094e820934809ei202
#define BREDY_HTTPCLIENT_HTTPSTREAM_H_2094e820934809ei202

#include <lightspeed/base/exceptions/exception.h>
#include <lightspeed/base/exceptions/ioexception.h>
#include <lightspeed/base/streams/fileio_ifc.h>
#include <lightspeed/base/text/textstream.h>

#include "../httpserver/headerValue.h"
#include "lightspeed/base/containers/stringpool.h"

#include "lightspeed/base/containers/map.h"
namespace LightSpeed {
class IInputBuffer;
}

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

	enum ReadHeaders {readHeadersNow};

	typedef BredyHttpSrv::HeaderValue HeaderValue;

	///Construct response
	/**
	 * @param com input stream
	 * @note Input stream must implement IInputBuffer, otherwise runtime error is reported.
	 * You should also reserve enough buffer to store single http header line
	 */
	HttpResponse(IInputStream *com);
	///Construct response
	/**
	 * @param com input stream
	 * @note Input stream must implement IInputBuffer, otherwise runtime error is reported.
	 * You should also reserve enough buffer to store single http header line
	 *
	 * Function also read headers
	 */
	HttpResponse(IInputStream *com, ReadHeaders);
	virtual ~HttpResponse();


	///Reads all headers. Function blocks until all headers are read.
	void readHeaders();

	///Returns status code
	/**
	 * @return status code. If status is unavailable, returns naturalNull
	 *
	 * @note status code is available after all headers has been read
	 */
	natural getStatus() const;

	///Returns status message
	/**
	 * @return status message. If status message is unavailable, returns empty string
	 *
	 * @note status message is available after all headers has been read
	 */
	ConstStrA getStatusMessage() const;


	HeaderValue getHeaderField(HeaderFieldDef::Field field) const;

	HeaderValue getHeaderField(ConstStrA field) const;

	bool isKeepAliveEnabled() const;

	natural getContentLength() const;

    virtual natural read(void *buffer,  natural size);
	virtual natural peek(void *buffer, natural size) const;
	virtual bool canRead() const;
	///Retrieves available data for reading
	/** If zero is returned, any operation with the stream will block. If non-zero is returned, you still need to call checkStream(),
	 * to process any required reading to keep http working.
	 *  If the function checkStream() returned true. then value of this function represents count of bytes ready to read by the read() operation
	 * @return
	 */
	virtual natural dataReady() const;

	///Skips any unreaded body
	/**
	 * @param async if true, function records the request and any following checkStream will continue to skip any body until end of stream
	 * is returned. If false, than function will block until all data are read.
	 */
	void skipRemainBody(bool async = false) ;

	///Checks stream, reads any bytes need to keep stream going
	/** @note function expects, that program already received notification about new data. Otherwise
	 * function can block
	 *
	 * @retval true stream is ready, you can read data - if you previously finished waiting, reading will not block
	 * @retval false stream is not ready yet, continue waiting
	 *
	 *
	 *
	 *
	 */
	bool checkStream();


	enum ReadingMode {
		///reading status header
		rmStatus,
		///reading headers
		rmHeaders,
		///reading is unlimited until end of stream is reached. Request is not keep-alive
		rmDirectUnlimited,
		///reading is limited - Content-Length in effect
		rmDirectLimited,
		///stream is reading header of a chunk. Reading is done during dataReady
		rmChunkHeader,
		///stream is reading chunk
		rmReadingChunk,
		///stream has been closed for reading
		rmEof,
		///Status 100 Continue received, waiting for next header
		rmContinue
	};

	enum TypeOfHeader {
		tohStatusLine,
		tohKeyValue,
		tohChunkSize
	};

	ReadingMode getReadingMode() const {return rMode;}

	///returns true, when server is currently waiting for body
	/** this can happen when Expect: 100-continue has been issued and server sent status 100.
	 * The function readHeaders() returns when 100 has been detetected. The function check stream
	 * returns 1 and the function read returns zero. Stream looks closed, however, state 'rmContinue'
	 * is issued, so object is ready to receive a new status and headers.
	 * @return
	 */
	bool canContinue() const {return rMode == rmContinue;}

	class MalformedResponse: public LightSpeed::IOException {
	public:
		MalformedResponse(const ProgramLocation &loc, TypeOfHeader section): Exception(loc),section(section) {}

		TypeOfHeader getSection() const {return section;}
		LIGHTSPEED_EXCEPTIONFINAL;
	protected:
		TypeOfHeader section;


		void message(ExceptionMsg &msg) const;
	};

	///Stream is incomplete
	class IncompleteStream: public LightSpeed::IOException {
	public:
		IncompleteStream(const ProgramLocation &loc): Exception(loc) {}

		LIGHTSPEED_EXCEPTIONFINAL;
	protected:


		void message(ExceptionMsg &msg) const;
	};


	IInputStream& getCom() {
		return com;
	}

	template<typename Fn>
	bool enumHeaders(const Fn &fn) const;

protected:
	IInputStream &com;


	typedef StringPool<char> StrPool;
	typedef StrPool::Str Str;

	StrPool pool;
	Str statusMessage;
	natural status;
	natural remainLength;
	ReadingMode rMode;
	AutoArray<char> buffer;
	Map<Str,Str> headers;
	bool http11;
	bool keepAlive;
	bool wBlock;
	bool skippingBody;
	IInputBuffer &ibuff;


	bool readHeaderLine(TypeOfHeader toh);
	bool endOfStream();
	bool processHeaders();
	bool invalidResponse(TypeOfHeader toh);
	bool processSingleHeader(TypeOfHeader toh, natural size);
	void runSkipBody();

};

template<typename Fn>
inline bool HttpResponse::enumHeaders(const Fn& fn) const {
	for (Map<Str,Str>::Iterator iter = headers.getFwIter(); iter.hasItems();) {
		const Map<Str,Str>::Entity &e = iter.getNext();
		if (!fn(ConstStrA(e.key),ConstStrA(e.value))) return false;
	}
	return true;
}


} /* namespace BredyHttpClient */


#endif /* BREDY_HTTPCLIENT_HTTPSTREAM_H_2094e820934809ei202 */
