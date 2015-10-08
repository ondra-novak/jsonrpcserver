#pragma once
#include "httprequest.h"
#include "lightspeed/base/debug/livelog.h"


namespace BredyHttpSrv {

	class LiveLogHandler: public IHttpHandler {
	public:
		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath);
		virtual natural onData(IHttpRequest &request);
		natural showFilterForm(IHttpRequest &request);
	};

	class LiveLogListener: public IHttpHandlerContext, public LiveLogStream {
	public:
		LiveLogListener(ILogOutput::LogType logLevel,PNetworkStream connection, bool asEventStream);

		class IOp;
		class Contains;
		class And;
		class Or;
		class Not;

		typedef RefCntPtr<IOp> POp;

		
		template<typename T>
		void writeConn(ConstStringT<T> data);

		virtual void onData(ConstStringT<byte> data);


		virtual void onClose();

		PNetworkStream connection;
		POp filter;
		bool asEventStream;

		typedef ErrorMessageException ParseError;

		void setFilter(ConstStrA filter);

	private:
		static POp parseFilter(ConstStrA::Iterator &filter, StringA &nextKw);
		static POp parseFilterAnd(ConstStrA::Iterator &filter, StringA &nextKw);
		static POp parseFilterValue(ConstStrA::Iterator &filter, StringA &nextKw);
		static POp parseFilterProcessValue(StringA value,bool iskw, ConstStrA::Iterator &fiter, StringA &nextKw);
		static StringA readNextItem(ConstStrA::Iterator &filter, bool &iskw);
	};


	class LiveLogAuthHandler: public LiveLogHandler {
	public:
		virtual natural onRequest(IHttpRequest &request, ConstStrA vpath);
		virtual natural onData(IHttpRequest &) {return 0;}

		void setRealm(StringA realm) {this->realm = realm;}
		void setUserlist(StringA userlist) {this->userlist = userlist;}

	protected:
		StringA realm;
		StringA userlist;
	};



}
