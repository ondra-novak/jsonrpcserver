#include "hostMapper.h"
#include "lightspeed\base\text\textParser.tcc"
#include "lightspeed\base\memory\smallAlloc.h"

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
		if (parser(" %1://%2%%[/](*)*3 -> %4 ", baseUrlFormat)) {
			data = baseUrlFormat;
			protocol = ConstStrA(parser[1].str()).map(data);
			host = ConstStrA(parser[2].str()).map(data);
			path = ConstStrA(parser[3].str()).map(data);
			targetVPath = ConstStrA(parser[4].str()).map(data);
		}
		else if (parser(" %1://%2%%[/](*)*3 ", baseUrlFormat)) {

			data = baseUrlFormat;
			protocol = ConstStrA(parser[1].str()).map(data);
			host = ConstStrA(parser[2].str()).map(data);
			path = ConstStrA(parser[3].str()).map(data);
			targetVPath = ConstStrA();

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
			throw NoMappingException(THISLOCATION);

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

	void HostMapper::NoMappingException::message(ExceptionMsg &msg) const
	{
		msg("No mapping for given host and path");
	}

	LightSpeed::ConstStrA HostMapper::MappingExistException::getHost() const
	{
		return host;
	}

	void HostMapper::MappingExistException::message(ExceptionMsg &msg) const
	{
		msg("Mapping already exists for the host %1") << host;
	}

}

