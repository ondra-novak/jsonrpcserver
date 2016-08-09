/*
 * pathMapper.h
 *
 *  Created on: Sep 18, 2012
 *      Author: ondra
 */

#ifndef JSONSERVER_PATHMAPPER_H_
#define JSONSERVER_PATHMAPPER_H_
#include "lightspeed/base/containers/string.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/containers/map.h"
#include "lightspeed/base/containers/set.h"
#include "lightspeed/base/containers/constStr.h"
#include "lightspeed/base/containers/autoArray.h"


namespace BredyHttpSrv {

class IHttpHandler;

using namespace LightSpeed;

class PathMapper {

	class PathOrder;

	typedef StringPool<char> SPool;
	typedef Map<SPool::Str, IHttpHandler *, std::greater<SPool::Str> > HandlerMap;


public:

	~PathMapper();
	void addHandler(ConstStrA path, IHttpHandler *h);
	void removeHandler(ConstStrA path);
	void clear();

	typedef HandlerMap::Iterator Iterator;
	typedef HandlerMap::Entity Record;

	Iterator getFwIter() const;


	class MappingIter: public IteratorBase<Record, MappingIter> {
	public:

		MappingIter(const HandlerMap &map, ConstStrA path);
		bool hasItems() const;
		const Record &getNext();
		const Record &peek();

	protected:
		const HandlerMap &map;
		ConstStrA path;
		const Record *fetched;

		void fetchNext();
		void fetchFirst();
	};

	MappingIter findPathMapping(ConstStrA path) const;

protected:

	HandlerMap handlers;
	SPool strings;


};

} /* namespace jsonsvc */
#endif /* JSONSERVER_PATHMAPPER_H_ */
