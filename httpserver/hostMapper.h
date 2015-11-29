#pragma once
#include "lightspeed\base\containers\constStr.h"
#include "lightspeed\base\containers\string.h"
#include "lightspeed\base\containers\stringKey.h"
#include "lightspeed\base\containers\map.h"



namespace BredyHttpSrv {

	using namespace LightSpeed;

	///maps host and path to the global virtual path
	class HostMapper {
	public:


		///register baseUrl or extended mapping
		/**
		 @param baseUrlFormat There can be full url to the webserver's root, for example http://example.com/aaa/bbb
		           this will register for the host example.com prefix /aaa/bbb and the path "" so any
				   request for example http://example.com/aaa/bbb/xxx/yyy.html will be mapped to the
				   vpath /xxx/yyy.html.
				   
				   
		There can be also following line:

		http://example.com/aaa/bbb -> /vhosts/example.com/x/y

		This line will map every request from above domain and path to specified vpath

		There can be also following line

		http://example.com /vhost/example.com

		This will map any request to example.com as /vhost/example.com. Because root request is always /
	    root vpath will be /vhost/example.com/

		You can use % instead host

		http://% -> /vhost/default

		this will map all request to undefined host to specified path

		@note http or https? Mapping will work for both. This server generally doesn't handle
		https, it must be handled by a upstream proxy. So you cannot map https to different vpath
		than http, because request are not encrypted. However, protocol is important when your
		handler uses getBaseUrl(), because it is emited here.
		*/
				   
		void registerUrl(ConstStrA baseUrlFormat);
		/** Manually register mapping

		@param host hostname
		@param protocol prefered protocol for getBaseUrl()
		@param path request path
		@param targetVPath target vpath
		*/
		void registerHost(ConstStrA host, ConstStrA protocol, ConstStrA path, ConstStrA targetVPath);

		///Maps path to vpath
		/**
		 @param host host field
		 @param path path field
		 @return new vpath

		 @exception NoMapping there is not available mapping. Server should return 404
		 */
		StringKey<StringA> mapRequest(ConstStrA host, ConstStrA path);

		///Returns base url depend on host
		StringA getBaseUrl(ConstStrA host);
		void unregisterUrl(ConstStrA mapLine);
		void processUrl(ConstStrA baseUrlFormat, bool param2);
		StringA getAbsoluteUrl(ConstStrA host, ConstStrA curPath, ConstStrA relpath);
		class NoMappingException : public Exception {
		public:
			LIGHTSPEED_EXCEPTIONFINAL;
			NoMappingException(const ProgramLocation &loc, StringA path) :Exception(loc) {}
		protected:
			void message(ExceptionMsg &msg) const;
			StringA path;
		};

		class MappingExistException : public Exception {
		public:
			LIGHTSPEED_EXCEPTIONFINAL;
			MappingExistException(const ProgramLocation &loc, StringA host) :Exception(loc),host(host) {}
			ConstStrA getHost() const;
		protected:
			StringA host;
			void message(ExceptionMsg &msg) const;
		};

		class MappingSyntaxError: public Exception {
		public:
			LIGHTSPEED_EXCEPTIONFINAL;
			MappingSyntaxError(const ProgramLocation &loc, StringA line) :Exception(loc), line(line) {}
			ConstStrA getLine() const;
		protected:
			StringA line;
			void message(ExceptionMsg &msg) const;
		};


	protected:

		struct Mapping {
			StringA data;
			ConstStrA protocol;
			ConstStrA path;
			ConstStrA targetVPath;

			Mapping(StringA data, ConstStrA protocol, ConstStrA path, ConstStrA targetVPath)
				:data(data), protocol(protocol), path(path), targetVPath(targetVPath) {};
			Mapping() {}
		};



		typedef Map<ConstStrA, Mapping> HostMapping;

		HostMapping mapping;
		Mapping defaultMapping;

	};

}