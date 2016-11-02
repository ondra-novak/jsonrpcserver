#include "livelog.h"

#include "lightspeed/base/exceptions/netExceptions.h"
#include "lightspeed/base/text/textstream.tcc"
#include "queryParser.h"
#include "lightspeed/base/containers/convertString.tcc"
#include "lightspeed/utils/base64.h"
#include "lightspeed/base/namedEnum.tcc"
#include "lightspeed/utils/json/jsonparser.tcc"
#include <lightspeed/base/interface.tcc>
#include "../jsonrpc/json.h"
#include <immujson/parser.h>


namespace BredyHttpSrv {


	class LiveLogListener::IOp: public RefCntObj {
	public:
		virtual bool execute(ConstStrA line) const = 0;
		virtual ~IOp() {}
	};


	class LiveLogListener::Contains: public IOp {
	public:
		Contains(StringA value):value(value) {}
		bool execute(ConstStrA line) const {
			return line.find(value) != naturalNull;
		}
	protected:
		StringA value;
	};

	class LiveLogListener::And: public IOp {
	public:
		And(POp op1, POp op2):op1(op1),op2(op2) {}
		bool execute(ConstStrA line) const {
			return op1->execute(line) && op2->execute(line);
		}
	protected:
		POp op1,op2;
	};

	class LiveLogListener::Or: public IOp {
	public:
		Or(POp op1, POp op2):op1(op1),op2(op2) {}
		bool execute(ConstStrA line) const {
			return op1->execute(line) || op2->execute(line);
		}
	protected:
		POp op1,op2;
	};

	class LiveLogListener::Not: public IOp {
	public:
		Not(POp op1):op1(op1) {}
		bool execute(ConstStrA line) const {
			return !op1->execute(line);
		}
	protected:
		POp op1,op2;
	};



	LiveLogListener::LiveLogListener( ILogOutput::LogType logLevel,PNetworkStream connection, bool asEventStream ) :LiveLogStream(logLevel),connection(connection), asEventStream(asEventStream)
	{

	}

	template<typename T>
	void LiveLogListener::writeConn( ConstStringT<T> data )
	{
		if (connection->wait(INetworkStream::waitForOutput,0) != 0) {
			connection->writeAll(data.data(),data.length());
		}
	}

	void LiveLogListener::onData( ConstStringT<byte> data )
	{
		if (filter == nil || filter->execute(ConstStrA((const char *)data.data(),data.length()))) {
			for (ConstStringT<byte>::SplitIterator iter = data.split('\n');iter.hasItems(); ) {
				ConstBin line = iter.getNext();
				for (ConstBin::SplitIterator iter2 = line.split('\r');iter2.hasItems();) {
					ConstBin line2 = iter2.getNext();
					if (!line2.empty()) {
						if (asEventStream) writeConn(ConstStrA("data: "));
						writeConn(line2);
						writeConn(ConstStrA("\r\n"));
					}
				}
			}
			if (asEventStream)
				writeConn(ConstStrA("\r\n"));;
		} else {
			if (connection->wait(INetworkStream::waitForInput,0) != 0)
				throw NetworkIOError(THISLOCATION,0,"Unexpected input");
		}
	}

	void LiveLogListener::onClose()
	{
		delete this;
	}

	StringA LiveLogListener::readNextItem(ConstStrA::Iterator &fiter, bool &iskw) {
		while (fiter.hasItems()) {
			char c = fiter.peek();
			if (!isspace(c)) {
				if (c == '"') {
					jsonrpc::JValue nd = jsonrpc::JValue::parse([&](){return fiter.getNext();});
					iskw = false;
					return ~nd.getString();
				} else {
					AutoArrayStream<char, SmallAlloc<256> > strBuilder;
					fiter.skip();
					do {
						strBuilder.write(c);
						if (fiter.hasItems())
							c = fiter.getNext();
						else
							c = 32;
					} while (!isspace(c));
					iskw = true;
					return strBuilder.getArray();
				}
			} else {
				fiter.skip();
			}
		}
		iskw = true;
		return StringA();
	}

	LiveLogListener::POp LiveLogListener::parseFilterValue(ConstStrA::Iterator &fiter, StringA &nextKw) {
		bool iskw;
		StringA s = readNextItem(fiter,iskw);
		return parseFilterProcessValue(s,iskw,fiter,nextKw);
	}
	LiveLogListener::POp LiveLogListener::parseFilterProcessValue(StringA s, bool iskw, ConstStrA::Iterator &fiter, StringA &nextKw) {
		POp r;
		if (!iskw) r = new Contains(s);
		else if (s == "(") {
			r = parseFilter(fiter,nextKw);
			if (nextKw != ")") throw ParseError(THISLOCATION,"Expecting )");
		} else if (s == "not") {
			r = parseFilterValue(fiter,nextKw);
			return new Not(r);
		} else {
			r = new Contains(s);
		}
		nextKw = readNextItem(fiter,iskw);
		if (!iskw || (nextKw != "and" && nextKw != "or" && !nextKw.empty())) {
			POp t = parseFilterProcessValue(nextKw,iskw,fiter,nextKw);
			return new And(r,t);
		} else {
			return r;
		}
	}

	LiveLogListener::POp LiveLogListener::parseFilterAnd(ConstStrA::Iterator &fiter, StringA &nextKw) {
		POp a= parseFilterValue(fiter,nextKw);
		if (nextKw == "and") {
			POp b = parseFilterAnd(fiter,nextKw);
			return new And(a,b);
		}
		return a;
	}

	LiveLogListener::POp LiveLogListener::parseFilter(ConstStrA::Iterator &fiter, StringA &nextKw) {
		POp a= parseFilterAnd(fiter,nextKw);
		if (nextKw == "or") {
			POp b = parseFilter(fiter,nextKw);
			return new Or(a,b);
		}
		return a;
	}

	void LiveLogListener::setFilter(ConstStrA f) {
		ConstStrA::Iterator iter=f.getFwIter();
		StringA nextKw;
		filter = parseFilter(iter,nextKw);
		if (!nextKw.empty()) throw ParseError(THISLOCATION,ConstStrA("Unexpected: ")+nextKw);
	}

	natural LiveLogHandler::showFilterForm(IHttpRequest &request) {
		SeqFileOutput out(&request);
		PrintTextA print(out);

		request.header(IHttpRequest::fldContentType,"text/html;charset=utf-8");
		print("<html><head><title>Enter filter</title></head>"
			"<script>\n"
			"var eventSource = null;\n"
			"function addToLog(data) {\n"
			"logBox.value += data + '\\n';\n"
			"if (document.getElementById('autoscroll').checked) logBox.scrollTop = 200000;\n"
			"if (logBox.value.length > 200000) \n"
			"   logBox.value = logBox.value.substr(logBox.value.length - 180000);\n"
			"}\n"
			"function init(path) {\n"
			"logBox = document.getElementById('log');\n"
			"if (eventSource != null) eventSource.close();"
			"eventSource = new EventSource(path);\n"
			"eventSource.onopen = function() {\n"
			"addToLog('onopen (readyState = ' + eventSource.readyState + ')');\n"
			"}\n"
			"eventSource.onmessage = function(event) {\n"
			"addToLog(event.data);\n"
			"}\n"
			"eventSource.onerror = function(event) {\n"
			"addToLog('onerror (readyState = ' + eventSource.readyState + ')');\n"
			"}\n"
			"}\n"
			"</script>\n"
			"<body onload=\"init('?')\">"
			"<table><tr><td>"
			"<div>Enter filter condition. Use operators "
			"<strong>and, or, not</strong>.<br /> You can also use <strong>( )</strong>.<br /> "
			"Complex strings can be enclosed in quotes.</div>"
			"<div style=\"font-size:smaller\">Example: foo and ( bar or fob )</div>"
			"</td><td>"
			"<textarea name=\"applyfilter\" rows=\"4\" cols=\"80\" id=\"filter\"></textarea>"
			"</td><td>"
			"<input type=\"button\" value=\"Apply filter\" onclick=\"init('?applyfilter='+encodeURIComponent(document.getElementById('filter').value));\" />"
			"<br><br><input type=\"checkbox\" id=\"autoscroll\" checked=\"checked\"> autoscroll"
			"</td></tr></table>"
			"<hr />"
			"<textarea id=\"log\" rows=\"40\" cols=\"40\" readonly style=\"width:100%%\"></textarea>"
			"</body>"
			"</html>");
		return 0;
	}

	static NamedEnumDef<ILogOutput::LogType> strLogTypeDef[] = {
		{ILogOutput::logFatal,"fatal"},
		{ILogOutput::logError,"error"},
		{ILogOutput::logWarning,"warning"},
		{ILogOutput::logNote,"note"},
		{ILogOutput::logProgress,"progress"},
		{ILogOutput::logInfo,"info"},
		{ILogOutput::logDebug,"debug"},
		{ILogOutput::logDebug,"all"}
	};
	static NamedEnum<ILogOutput::LogType> strLogType(strLogTypeDef);

	 natural LiveLogHandler::onRequest(IHttpRequest& request,ConstStrA vpath) {
		QueryParser parser(vpath);

		LogOutputSingleton curLogS = ILogOutput::getLogOutputSingleton();
		ILogOutput &logOutput = curLogS();
		LiveLog *liveLog = logOutput.getIfcPtr<LiveLog>();
		if (liveLog == 0) return stNotFound;


		ConstStrA levelDesc = parser.getPath();

		if (levelDesc.empty()) {
			request.redirect("+/");
			return stOK;
		}

		if (levelDesc == "/") {
			request.header(IHttpRequest::fldContentType,"text/html;charset=UTF-8");
			SeqFileOutput out(&request);
			PrintTextA print(out);
			print("<!DOCTYPE html><html><body><ul>");
			for (natural i = 0; i < 7; i++) {
				print("<li><a href=\"%1?filter\">%1</a></li>") << ConstStrA(strLogType[(ILogOutput::LogType)(i+1)]);
			}
			print("</ul></body></html>");
			return 0;
		}


		ILogOutput::LogType t;
		levelDesc = levelDesc.offset(1);
		try {
			t = strLogType[levelDesc];
		} catch(UnknownEnumName &e) {
			request.errorPage(404,"Not found",e.what())	;			
			return 0;
		}

		HeaderValue accept = request.getHeaderField(IHttpRequest::fldAccept);
		bool asEventStream = accept.defined && accept == "text/event-stream";

		StringA strfilter;
		while (parser.hasItems()) {
			const QueryField &field = parser.getNext();
			if (field.name == "applyfilter") {
				strfilter = field.value;
			} else if (field.name == "filter") {
				return showFilterForm(request);
			}
		}



		request.useHTTP11(false);
		if (asEventStream)
			request.header(IHttpRequest::fldContentType,"text/event-stream;charset=UTF-8");
		else
			request.header(IHttpRequest::fldContentType,"text/plain;charset=UTF-8");

		request.header(IHttpRequest::fldConnection,"close");
		request.sendHeaders();

		LiveLogListener *l = new LiveLogListener(t,request.getConnection(),asEventStream);

		SeqFileOutput out(&request);
		PrintTextA print(out);
		print.setNL("\r\n");
		print("Connected to livelog service - log level is set to: %1\n") << levelDesc;

		if (!asEventStream) {
			if (strfilter.empty()) {
				print("Hint: You can enable filtering, try add \"?filter\" at the end of URL\n");
			} else {
				try {
					l->setFilter(strfilter);
					print("Note: Filter applied\n");
				} catch (std::exception &e) {
					print("Filter error: %1\n") << e.what();
				}
			}
		} else if (!strfilter.empty()){
			try {
				l->setFilter(strfilter);
				print("data: Note: Filter applied\n\n");
			} catch (std::exception &e) {
				print("data: Filter error: %1\n\n") << e.what();
			}
		}

		liveLog->add(l);
		return stOK;
	}

	natural LiveLogHandler::onData(IHttpRequest& ) {
		return 0;
	}

	natural LiveLogAuthHandler::onRequest(IHttpRequest &request, ConstStrA vpath) {
		HeaderValue auth = request.getHeaderField(IHttpRequest::fldAuthorization);
		if (auth.defined && vpath!="/logout") {
			StringCore<byte> bident = convertString<Base64Decoder,char,byte>(Base64Decoder(),auth.offset(6));
			ConstStrA ident((const char *)bident.data(),bident.length());
			for (StringA::SplitIterator uiter = userlist.split(','); uiter.hasItems();) {
				ConstStrA user = uiter.getNext();
				if (user == ident) {
					return LiveLogHandler::onRequest(request,vpath);
				}
			}
		}
		request.header(IHttpRequest::fldWWWAuthenticate,
			StringA(ConstStrA("Basic realm=\"") + realm + ConstStrA("\"")));
		return 401;
	}

}
