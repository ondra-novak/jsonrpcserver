/*
 * rpchadler.cpp
 *
 *  Created on: 26. 5. 2014
 *      Author: ondra
 */


#include "lightspeed/base/containers/constStr.h"
#include <lightspeed/base/memory/refCntPtr.h>
#include "rpchandler.h"



namespace jsonsrv {


ConstStrA RpcRequest::argStrA(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node->getStringUtf8();
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
ConstStrW RpcRequest::argStrW(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node->getString();
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
integer RpcRequest::argInt(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node->getInt();
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
natural RpcRequest::argUInt(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node->getUInt();
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
double RpcRequest::argFloat(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node->getFloat();
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
bool RpcRequest::argBool(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node->getBool();
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
JSON::PNode RpcRequest::argObject(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) return iter.getNext().node;
	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}
///Tests, whethe next argument is specified name and is NULL
/** In contrast to other functions, this doesn't advance to next argument when
* argument is not NULL.
* @param iter iterator
* @param name name of argument
* @retval true argument is NULL, advanced to next argument
* @retval false argument is not NULL, not advanced - you have to read value
*/
bool RpcRequest::isNull(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) {
		if (iter.peek().node->isNull()) {
			iter.skip();
			return true;
		} else {
			return false;
		}
	}

	else throw RpcMandatoryField(THISLOCATION,jsonFactory,name);
}

ConstStrA RpcRequest::argStrA(ArgIter &iter, ConstStrA name, ConstStrA defval) {
	if (iter.isNextKey(name)) return iter.getNext().node->getStringUtf8();
	else return defval;
}
ConstStrW RpcRequest::argStrW(ArgIter &iter, ConstStrA name, ConstStrW defval) {
	if (iter.isNextKey(name)) return iter.getNext().node->getString();
	else return defval;
}
integer RpcRequest::argInt(ArgIter &iter, ConstStrA name, integer defval) {
	if (iter.isNextKey(name)) return iter.getNext().node->getInt();
	else return defval;
}
natural RpcRequest::argUInt(ArgIter &iter, ConstStrA name, natural defval) {
	if (iter.isNextKey(name)) return iter.getNext().node->getUInt();
	else return defval;
}
double RpcRequest::argFloat(ArgIter &iter, ConstStrA name, double defval) {
	if (iter.isNextKey(name)) return iter.getNext().node->getFloat();
	else return defval;
}
bool RpcRequest::argBool(ArgIter &iter, ConstStrA name, bool defval) {
	if (iter.isNextKey(name)) return iter.getNext().node->getBool();
	else return defval;
}
JSON::PNode RpcRequest::argObject(ArgIter &iter, ConstStrA name, JSON::PNode defval) {
	if (iter.isNextKey(name)) return iter.getNext().node;
	else return defval;
}
bool RpcRequest::hasValue(ArgIter &iter, ConstStrA name) {
	if (iter.isNextKey(name)) {
		if (iter.peek().node->isNull()) {
			iter.skip();
			return true;
		} else {
			return false;
		}
	}

	else return false;
}


}

