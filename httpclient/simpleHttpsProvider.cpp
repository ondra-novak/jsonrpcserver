/*
 * simpleHttpsProvider.cpp
 *
 *  Created on: 2. 5. 2016
 *      Author: ondra
 */

#include <lightspeed/base/exceptions/systemException.h>
#include <lightspeed/base/exceptions/throws.tcc>
#include <lightspeed/base/exceptions/unsupportedFeature.h>
#include "interfaces.h"
#include "lightspeed/mt/fastlock.h"
#include "simpleHttpsProvider.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/exceptions/errorMessageException.h"
#include "lightspeed/mt/exceptions/timeoutException.h"

#include "../config.h"
#if HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace BredyHttpClient {

using namespace LightSpeed;



class SSLSocket_t: public INetworkStream, public INetworkSocket {
public:
	SSLSocket_t( const SSL_METHOD *method);
	~SSLSocket_t();

	void connect(PNetworkStream connectedStream);

	void loadVerifyLocations(ConstStrA caFile, ConstStrA caPath);
	void loadCertifikatePEM(ConstStrA fileName);
	void loadCertifikateChainPEM(ConstStrA fileName);
	void loadPrivKeyPEM(ConstStrA fileName);
	void loadDefaultCertLocations();

    virtual natural read(void *buffer,  natural size) ;
    virtual natural write(const void *buffer,  natural size);
    virtual natural peek(void *buffer, natural size) const;
    virtual bool canRead() const ;
    virtual bool canWrite() const ;
    virtual void flush() ;
    virtual natural dataReady() const ;
	virtual natural getDefaultWait() const;
	virtual void setWaitHandler(WaitHandler *handler) ;
	virtual WaitHandler *getWaitHandler() const {return connectedStream->getWaitHandler();}
	virtual void setTimeout(natural time_in_ms) ;
	virtual natural getTimeout() const ;
	virtual natural wait(natural waitFor, natural timeout) const ;
	virtual void closeOutput();



	virtual integer getSocket(int index) const;

	void setHostName(ConstStrA hostname);


protected:
	SSL *ssl;
	SSL_CTX *ctx;
	PNetworkStream connectedStream;
	StringA hostname;

	virtual natural doWait(natural waitFor, natural timeout) const {
		WaitHandler tmp;
		return tmp.wait(connectedStream,waitFor,timeout);
	}

	///globalni lock - libssl MT safety issues.
	static FastLock globalGuard;

	void waitSSLSocket(int err);

};

class SSLException_t: public ErrNoWithDescException {
public:
	LIGHTSPEED_EXCEPTIONFINAL;
	SSLException_t(const ProgramLocation &loc,
			natural errCode,
			ConstStrA desc);

protected:
	void message( ExceptionMsg &msg) const ;
};





PNetworkStream BredyHttpClient::SimpleHttps::connectTLS( PNetworkStream connection, ConstStrA hostname) {

	SSLSocket_t *sock = new SSLSocket_t(TLSv1_2_client_method());
	PNetworkStream stream = sock;
	sock->setHostName(hostname);
	sock->loadDefaultCertLocations();
	sock->connect(connection);
	return stream;

	//SSL_set_tlsext_host_name()
}

SimpleHttps* BredyHttpClient::SimpleHttps::getInstance() {
	static SimpleHttps instance;
	return &instance;
}



class SSLInit_t {
public:
	SSLInit_t() {
	    /* Load encryption & hashing algorithms for the SSL program */
	    SSL_library_init();

	    /* Load the error strings for SSL & CRYPTO APIs */
	    SSL_load_error_strings();
	}
};




SSLSocket_t::SSLSocket_t(const SSL_METHOD *method)
	:ssl(0)
{
	Singleton<SSLInit_t>::getInstance();
	ctx = SSL_CTX_new(const_cast<SSL_METHOD *>(method));
}

SSLSocket_t::~SSLSocket_t() {
	if (ssl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	if (ctx) SSL_CTX_free(ctx);
}

typedef AutoArrayStream<char> SSLError_t;
static int fetchSSLError(const char *str, size_t len, void *u) {
	 SSLError_t *aas = (SSLError_t *)u;
	 aas->blockWrite(ConstStrA(str,len),true);
	 return len;
}

static void throwSSLError(const ProgramLocation &loc) {
	SSLError_t e;
	unsigned long err = ERR_get_error();
	ERR_print_errors_cb(fetchSSLError,&e);
	ERR_clear_error();
	throw SSLException_t(loc,err,ConstStrA(e.getArray()));

}

static void throwSSLError(const ProgramLocation &loc, SSL *ssl, int errres) {
	SSLError_t e;
	unsigned long err = SSL_get_error(ssl,errres);
	ERR_print_errors_cb(fetchSSLError,&e);
	ERR_clear_error();
	throw SSLException_t(loc,err,ConstStrA(e.getArray()));

}

void SSLSocket_t::loadVerifyLocations(ConstStrA caFile, ConstStrA caPath) {
	StringA tmp1,tmp2;

	if (SSL_CTX_load_verify_locations(ctx
			,caFile.empty()?NULL:cStr(caFile,tmp1)
			,caPath.empty()?NULL:cStr(caPath,tmp2)) <= 0) {
		throwSSLError(THISLOCATION);
	}
}


void SSLSocket_t::connect(LightSpeed::PNetworkStream connectedStream) {
	if (ssl != 0) throw ErrorMessageException(THISLOCATION,"Socket is already connected");
	ssl = SSL_new(ctx);
	if (ssl == 0) throw ErrNoWithDescException(THISLOCATION,errno,"Unable to create SSL descriptor");

	if (!hostname.empty())
		SSL_set_tlsext_host_name(ssl,hostname.cStr());

	INetworkSocket &sockIfc = connectedStream->getIfc<INetworkSocket>();
	bool done = false;
	int socket = sockIfc.getSocket(0);

//	u_long nonblk =1;
//	ioctl(socket,FIONBIO, &nonblk);

	SSL_set_fd(ssl,socket);

	this->connectedStream = connectedStream;
	while (!done) {
		int err = SSL_connect(ssl);
		if (err <= 0) {
			waitSSLSocket(err);
		} else {
			done = true;
		}
	}


}

SSLException_t::SSLException_t(const LightSpeed::ProgramLocation& loc,
		LightSpeed::natural errCode,
		LightSpeed::ConstStrA desc)
	:ErrNoWithDescException(loc,errCode,desc)
{
}

void SSLException_t::message(
		 LightSpeed::ExceptionMsg& msg) const {
	msg("SSLError: %1 %2") << getErrNo() << getDesc();
}

void SSLSocket_t::loadCertifikatePEM(LightSpeed::ConstStrA fileName) {
	Synchronized<FastLock> _(globalGuard);
	StringA tmp;
	if (SSL_CTX_use_certificate_file(ctx,cStr(fileName,tmp),SSL_FILETYPE_PEM) <= 0) {
		throwSSLError(THISLOCATION);
	}
}

void SSLSocket_t::loadCertifikateChainPEM(LightSpeed::ConstStrA fileName) {
	Synchronized<FastLock> _(globalGuard);
	StringA tmp;
	if (SSL_CTX_use_certificate_chain_file(ctx,cStr(fileName,tmp)) <= 0) {
		throwSSLError(THISLOCATION);
	}
}


void SSLSocket_t::loadPrivKeyPEM(LightSpeed::ConstStrA fileName) {
	Synchronized<FastLock> _(globalGuard);
	StringA tmp;
	if (SSL_CTX_use_PrivateKey_file(ctx,cStr(fileName,tmp),SSL_FILETYPE_PEM) <= 0) {
		throwSSLError(THISLOCATION);
	}

}

natural SSLSocket_t::read(void* buffer, natural size) {
	if (size == 0) return 0;
	do {
		int res = SSL_read(ssl,buffer,size);
		if (res >= 0) return res;
		waitSSLSocket(res);
	} while (true);
}

natural SSLSocket_t::write(const void* buffer, natural size) {
	if (size == 0) return size;
	do {
		int res = SSL_write(ssl,buffer,size);
		if (res >= 0) return res;
		waitSSLSocket(res);
	} while (true);
}

natural SSLSocket_t::peek(void* buffer, natural size) const {
	if (buffer == 0) {
		byte tmp[256];
		return peek(tmp,256);
	} else {
		do {
			int res = SSL_peek(ssl,buffer,size);
			if (res >= 0) return res;
			const_cast<SSLSocket_t *>(this)->waitSSLSocket(res);
		} while (true);
	}

}

bool SSLSocket_t::canRead() const {
	return true;
}

bool SSLSocket_t::canWrite() const {
	return true;
}

void SSLSocket_t::flush() {
	connectedStream->flush();
}

natural SSLSocket_t::dataReady() const {
	return connectedStream->dataReady();
}

natural SSLSocket_t::getDefaultWait() const {
	return 300*1000;
}

void SSLSocket_t::setWaitHandler(WaitHandler*h ) {
	connectedStream->setWaitHandler(h);
}

void SSLSocket_t::setTimeout(natural t) {
	connectedStream->setTimeout(t);
}

natural SSLSocket_t::getTimeout() const {
	return 300*1000;
}

natural SSLSocket_t::wait(natural waitFor, natural t) const {
	return connectedStream->wait(waitFor,t);
}

integer SSLSocket_t::getSocket(int index) const {
	if (connectedStream == nil) return -1;
	return connectedStream->getIfc<INetworkSocket>().getSocket(index);
}

inline void SSLSocket_t::loadDefaultCertLocations() {
	SSL_CTX_set_default_verify_paths(ctx);
}


inline void SSLSocket_t::setHostName(ConstStrA hostname) {
	this->hostname = hostname;
}

void SSLSocket_t::waitSSLSocket(int err) {
	int code = SSL_get_error(ssl,err);
	if (code == SSL_ERROR_WANT_READ) {
		if (connectedStream->wait(INetworkResource::waitForInput) == INetworkResource::waitTimeout)
			throw TimeoutException(THISLOCATION);
	} else if (code == SSL_ERROR_WANT_WRITE) {
		if (connectedStream->wait(INetworkResource::waitForOutput) == INetworkResource::waitTimeout)
			throw TimeoutException(THISLOCATION);
	} else {
		throwSSLError(THISLOCATION,ssl,err);
	}
}

void SSLSocket_t::closeOutput() {
}

FastLock SSLSocket_t::globalGuard;


}
#else

namespace BredyHttpClient {

using namespace LightSpeed;

PNetworkStream BredyHttpClient::SimpleHttps::connectTLS( PNetworkStream connection, ConstStrA hostname) {
	throw UnsupportedFeatureOnClass<SimpleHttps>("This library wasn't compiled with openssl",THISLOCATION);
}

SimpleHttps* BredyHttpClient::SimpleHttps::getInstance() {
	throw UnsupportedFeatureOnClass<SimpleHttps>("This library wasn't compiled with openssl",THISLOCATION);
}


}
#endif
