/*
 * pathMapper.cpp
 *
 *  Created on: Sep 18, 2012
 *      Author: ondra
 */

#include "pathMapper.h"
#include "lightspeed/base/containers/stringpool.tcc"
#include "lightspeed/base/debug/dbglog.h"

namespace BredyHttpSrv {



void PathMapper::addHandler(ConstStrA path, IHttpHandler* h) {

	bool found;
	Iterator i = handlers.seek(path,&found);
	if (found) {
		const Record &r = i.getNext();
		r.value = h;
	} else
		handlers.insert(strings(path),h);
}

void PathMapper::removeHandler(ConstStrA path) {
	handlers.erase(path);
	if (handlers.empty()) {
		strings.clear();
	}
}

PathMapper::Iterator PathMapper::getFwIter() const {
	return handlers.getFwIter();
}

PathMapper::MappingIter::MappingIter(const HandlerMap& map,
		ConstStrA path):map(map),path(path) {
	natural k = this->path.find('?');
	if (k != naturalNull) this->path = this->path.head(k);
	fetchFirst();
}

bool PathMapper::MappingIter::hasItems() const {
	return fetched != 0;
}

const PathMapper::Record& PathMapper::MappingIter::getNext() {
	const PathMapper::Record *r = fetched;
	fetchNext();
	return *r;
}

const PathMapper::Record& PathMapper::MappingIter::peek() {
	return *fetched;
}

void PathMapper::MappingIter::fetchNext() {
	natural p = path.findLast('/');
	if (p == naturalNull) fetched = 0;
	else {
		path = path.head(p);
		fetchFirst();
	}
}

void PathMapper::MappingIter::fetchFirst() {
	bool found;
	HandlerMap::Iterator iter = map.seek(path,&found);
	if (found) {
		fetched = &iter.getNext();
	} else {
		fetchNext();
	}
}

void PathMapper::clear() {
	handlers.clear();
	strings.clear();
}

PathMapper::MappingIter PathMapper::findPathMapping(ConstStrA path) const {
	return MappingIter(handlers,path);
}

/* namespace jsonsvc */

}

template class LightSpeed::StringPool<char>;

