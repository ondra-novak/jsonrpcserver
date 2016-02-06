#include "hostMapper.h"
#include "lightspeed/base/text/textParser.tcc"
#include "lightspeed/base/memory/smallAlloc.h"

namespace BredyHttpSrv {

	void HostMapper::registerUrl(ConstStrA baseUrlFormat)
	{
		processUrl(baseUrlFormat, false);
	}
	void HostMapper::processUrl(ConstStrA baseUrlFormat, bool unreg)
	{
		TextParser<char, SmallAlloc<256> > parser;
		StringA data;
		ConstStrA host;
		ConstStrA path;
		ConstStrA protocol;
		ConstStrA targetVPath;
		if (parser(" %[a-zA-Z0-9]1://%[a-zA-Z0-9-._:]2%%[/](*)[*^> ]3 -> %4 ", baseUrlFormat)) {
			data = baseUrlFormat;
			protocol = ConstStrA(parser[1].str()).map(data);
			host = ConstStrA(parser[2].str()).map(data);
			path = ConstStrA(parser[3].str()).map(data);
			targetVPath = ConstStrA(parser[4].str()).map(data);
		}
		else if (parser(" %[a-zA-Z0-9]1://%[a-zA-Z0-9-._:]2%%[/](*)[^> ]3 ", baseUrlFormat)) {

			data = baseUrlFormat;
			protocol = ConstStrA(parser[1].str()).map(data);
			host = ConstStrA(parser[2].str()).map(data);
			path = ConstStrA(parser[3].str()).map(data);
			targetVPath = (path.empty() || path.tail(1) == ConstStrA("/"))?ConstStrA("/"):ConstStrA();

		}
		else {
			throw MappingSyntaxError(THISLOCATION, baseUrlFormat);
		}
		if (host == "%") {
			if (unreg)
				defaultMapping = Mapping();
			else
				defaultMapping = Mapping(data, protocol, path, targetVPath);
		}
		else {
			if (unreg) {
				mapping.erase(host);
			}
			else {
				mapping.replace(host, Mapping(data, protocol, path, targetVPath));
			}
		}
	}


	void HostMapper::registerHost(ConstStrA host, ConstStrA protocol, ConstStrA path, ConstStrA targetVPath)
	{
		StringA data = host + protocol + path + targetVPath;
		host = data.head(host.length());
		protocol = data.mid(host.length(), protocol.length());
		path = data.mid(host.length() + protocol.length(), path.length());
		targetVPath = data.mid(host.length() + protocol.length() + path.length(), targetVPath.length());

		if (host == "%") defaultMapping = Mapping(data, protocol, path, targetVPath);
		else mapping.replace(host, Mapping(data, protocol, path, targetVPath));
	}

	StringKey<StringA> HostMapper::mapRequest(ConstStrA host, ConstStrA path)
	{
		const Mapping *mp = mapping.find(host);
		if (mp == 0)
			mp = &defaultMapping;		

		if (path.head(mp->path.length()) != mp->path) 
			throw NoMappingException(THISLOCATION, StringA(host+path));

		if (mp->targetVPath.empty())
			return StringKey<StringA>(ConstStrA(path.offset(mp->path.length())));
		else
			return  StringKey<StringA>(StringA(mp->targetVPath + path.offset(mp->path.length())));
			
	}

	LightSpeed::StringA HostMapper::getBaseUrl(ConstStrA host)
	{
		const Mapping *mp = mapping.find(host);
		if (mp == 0)
			mp = &defaultMapping;
		return StringA(mp->protocol + ConstStrA("://") + host + mp->path);

	}

	void HostMapper::unregisterUrl(ConstStrA mapLine)
	{
		processUrl(mapLine, true);
	}

	LightSpeed::StringA HostMapper::getAbsoluteUrl(ConstStrA host, ConstStrA curPath, ConstStrA relpath)
	{
		const Mapping *mp = mapping.find(host);
		if (mp == 0)
			mp = &defaultMapping;

		StringA tmp;

		if (relpath.empty()) {
			//if relpath is empty, we can directly create url using curent path
			//combine protocol://host/curpath
			return StringA(mp->protocol + ConstStrA("://") + host + curPath);
		}
		else if (relpath.head(2) == ConstStrA("+/")) {
			natural q = relpath.find('?');
			natural p = curPath.find('?');
			ConstStrA query;
			if (p != naturalNull) {
				query = curPath.offset(p);
				curPath = curPath.head(p);
			};
			if (q != naturalNull) query.empty();
			return StringA(mp->protocol + ConstStrA("://") + host + curPath + relpath.offset(1)+query);
		}

		else if (relpath[0] == '/') {
			//path relative to host root
			//path must respect current vpath mapping 
			//so if host's root is mapped to some vpath, we need to remove that part from the relpath
			//check whether this is valid request
			//in context of host, function cannot create url to the paths outside of mapping
			if (relpath.head(mp->targetVPath.length()) == mp->targetVPath) {
				//now adjust host's relpath
				relpath = relpath.offset(mp->targetVPath.length());
				//no add host's path from the mapping
				if (!mp->path.empty()) {
					//check path for ending slash
					bool endslash = mp->path.tail(1) == ConstStrA('/');
					//check relpath for starting slash
					bool startslash = relpath.head(1) == ConstStrA('/');
					//if both have slash
					if (endslash &&  startslash) {
						//remove starting slash and combine path
						tmp = mp->path + relpath.offset(1);					
					}
					//neither both have slash
					else if (!endslash && !startslash)  {
						//put slash between path and relpath
						tmp = mp->path + ConstStrA('/') + relpath;
					}
					else {
						//combine paths
						tmp = mp->path + relpath;
					}					
					relpath = tmp;
				}

				//no we have path
				// 1. removed vpath offset from mapping
				// 2. added host's offset from mapping 
				// (reversed mapping)
				
				//finally, check, whether there is slash at begging
				if (relpath.head(1) == ConstStrA('/')) {
					//if not. pretend one '/'
					tmp = ConstStrA("/") + relpath;
					relpath = tmp;
				}
				//combine protocol://host/relpath
				return StringA(mp->protocol + ConstStrA("://") + host + relpath);
			}
			else {
				throw NoMappingException(THISLOCATION,relpath);
			}
		}
		else {
			//remove query: /aa/bbb/c?x=0 -> /aa/bbb/c
			natural query = curPath.find('?');
			if (query != naturalNull) curPath = curPath.head(query);

			//remove filename: /aa/bbb/c -> /aa/bbb/
			natural slash = curPath.findLast('/');
			curPath = curPath.head(slash + 1);

			//combine protocol://host/path/relpath
			return StringA(mp->protocol + ConstStrA("://") + host + curPath + relpath);
		}
	}

	void HostMapper::NoMappingException::message(ExceptionMsg &msg) const
	{
		msg("No mapping for given host and path: %1") << path;
	}

	LightSpeed::ConstStrA HostMapper::MappingExistException::getHost() const
	{
		return host;
	}

	void HostMapper::MappingExistException::message(ExceptionMsg &msg) const
	{
		msg("Mapping already exists for the host %1") << host;
	}

	void HostMapper::MappingSyntaxError::message(ExceptionMsg &msg) const
	{
		msg("Host mapping - syntax error: %1") << line;
	}

}

