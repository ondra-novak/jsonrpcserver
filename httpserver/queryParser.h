/*
 * queryParser.h
 *
 *  Created on: 24.9.2013
 *      Author: ondra
 */

#ifndef JSONSERVER_QUERYPARSER_H_
#define JSONSERVER_QUERYPARSER_H_
#include "lightspeed/base/containers/autoArray.h"
#include "lightspeed/base/containers/constStr.h"
#include "lightspeed/base/iter/iterator.h"
#include "lightspeed/base/memory/smallAlloc.h"

namespace BredyHttpSrv {

	struct QueryField {

		LightSpeed::ConstStrA name;
		LightSpeed::ConstStrA value;

	};

	class QueryParser: public LightSpeed::IteratorBase<QueryField,QueryParser> {
	public:

		QueryParser(LightSpeed::ConstStrA vpath);

		LightSpeed::ConstStrA getPath() const;
		LightSpeed::ConstStrA getQuery() const;
		const QueryField &getNext();
		const QueryField &peek() const;
		bool hasItems() const;

	protected:

		LightSpeed::ConstStrA _path;
		LightSpeed::ConstStrA _query;
		LightSpeed::AutoArrayStream<char, LightSpeed::SmallAlloc<100> > _tempVal[2];
		QueryField _tmpResult[2];
		LightSpeed::ConstStrA::SplitIterator _iter;
		bool _hasNext;
		int _swp;

		void preload();

	};


}


#endif /* JSONSERVER_QUERYPARSER_H_ */

