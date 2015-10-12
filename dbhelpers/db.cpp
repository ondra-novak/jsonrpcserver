/*
 * db.cpp
 *
 *  Created on: Dec 28, 2012
 *      Author: ondra
 */


#include "db.h"
#include "lightmysql/result.h"
#include <mysql/mysql.h>
#include "lightspeed/base/text/textParser.tcc"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/memory/staticAlloc.h"
#include "lightspeed/base/timestamp.h"
#include "lightspeed/base/exceptions/errorMessageException.h"
#include "lightspeed/base/debug/dbglog.h"
#include "../jsonrpc/base64.h"

using LightSpeed::TimeStamp;

namespace jsonsrv {


const char *message_InvalidDateTimeFormat = "Invalid date/time: %1";

TimeStamp parseDateTime(ConstStrA text) {
	using namespace LightSpeed;
	TextParser<char, StaticAlloc<256> > parser;
	if (parser("%u1-%u2-%u3 %u4:%u5:%u6",text)) {
		natural yr = parser[1],
				mh = parser[2],
				dy = parser[3],
				hh = parser[4],
				mn = parser[5],
				sc = parser[6];
			return TimeStamp::fromYMDhms(yr,mh,dy,hh,mn,sc);
	} else if (parser("%u1-%u2-%u3",text)) {
		natural yr = parser[1],
				mh = parser[2],
				dy = parser[3];
		return TimeStamp::fromYMDhms(yr,mh,dy,0,0,0);
	} else if (parser("%u1:%u2:%u3",text)) {
		natural hh = parser[1],
				mn = parser[2],
				sc = parser[3];
		return TimeStamp::fromYMDhms(0,0,0,hh,mn,sc);
	} else {
		throw InvalidDateTimeFormat(THISLOCATION, text);
	}


}

LightSpeed::JSON::PNode parseDateTime(LightSpeed::JSON::IFactory &factory, ConstStrA text, bool strDate) {
	using namespace LightSpeed;

	if (strDate) return factory.newValue(text);

	try {
		TimeStamp tms = parseDateTime(text);
		return factory.newValue(tms.getFloat());
	} catch (const InvalidDateTimeFormat &e) {
		return factory.newValue(e.getMessage());
	}

}


LightSpeed::JSON::PNode createArrayFromSet(LightSpeed::JSON::IFactory &factory, ConstStrA text, char sep) {
	LightSpeed::JSON::PNode arr = factory.newArray();
	if (text.empty()) return arr;

	for (ConstStrA::SplitIterator iter = text.split(sep);iter.hasItems();) {
		ConstStrA name = iter.getNext();
		if (!name.empty()) {
			arr->add(factory.newValue(name));
		}
	}
	return arr;
}


LightSpeed::JSON::PNode dbResultToJsonObject(
		LightSpeed::JSON::IFactory& factory, LightMySQL::Row& row, bool strDate) {

	using namespace LightSpeed::JSON;
	using namespace LightSpeed;

	PNode obj = factory.newClass();
	natural c = row.size();
	for (natural i = 0; i < c; i++) {
		const MYSQL_FIELD *fld = row.getResultObject().getFieldInfo(i);
		ConstStrA name(fld->name,fld->name_length);
		PNode nd = nil;
		if (row[i].isNull()) {
			nd = factory.newNullNode();
		} else {
			switch (fld->type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_LONGLONG:
			case MYSQL_TYPE_INT24: nd = factory.newValue(row[i].as<integer>());break;

			case MYSQL_TYPE_TIMESTAMP:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_NEWDATE: nd = parseDateTime(factory,row[i].as<ConstStrA>(),strDate);break;

			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE: nd = factory.newValue(row[i].as<double>());break;

/*			case MYSQL_TYPE_TINY_BLOB:
			case MYSQL_TYPE_MEDIUM_BLOB:
			case MYSQL_TYPE_LONG_BLOB:
			case MYSQL_TYPE_BLOB:nd = factory.newValue(base64_encode(row[i].as<ConstStrA>()));
								break;*/

			case MYSQL_TYPE_SET: nd = createArrayFromSet(factory,row[i].as<ConstStrA>());break;
			default: nd =  factory.newValue(row[i].as<ConstStrA>());
					break;
			}
		}

		obj->add(name,nd);


	}
	return obj;


}




void DBResultToJSON::init(LightSpeed::natural count, const LightMySQL::Result& result, bool hideAl) {
	using namespace LightSpeed;
	for (natural i = 0; i < count; i++) {
		const MYSQL_FIELD* fld = result.getFieldInfo(i);
		ConstStrA name(fld->name, fld->name_length);
		FieldFormat format;
		if (hideAl) {
			format = skip;
		} else {
			switch (fld->type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_LONGLONG:
			case MYSQL_TYPE_INT24:
				format = integer;
				break;
			case MYSQL_TYPE_TIMESTAMP:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_NEWDATE:
				format = datetime;
				break;
			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
				format = floatnum;
				break;
			case MYSQL_TYPE_SET:
				format = set;
				break;
			default:
				format = string;
			}
		}
		rowDesc.add(RowItem(name, format));
	}
}

DBResultToJSON::DBResultToJSON(LightSpeed::JSON::IFactory* factory, const LightMySQL::Result& result, bool strDate)
	:factory(factory),result(result),strDate(strDate),buildArray(false)
{
	using namespace LightSpeed;
	natural count = result.getFieldCount();
	rowDesc.reserve(count);
	init(count, result, false);
}

DBResultToJSON::DBResultToJSON(LightSpeed::JSON::IFactory *factory, const LightMySQL::Result &result, bool strDate, bool noInit, bool buildArray)
	:factory(factory),result(result),strDate(strDate),buildArray(buildArray)
{
	using namespace LightSpeed;
	natural count = result.getFieldCount();
	rowDesc.reserve(count);
	init(count, result, noInit);

}


void jsonsrv::DBResultToJSON::setFormat(LightSpeed::natural index, FieldFormat fld) {
	rowDesc(index).format = fld;
}

void jsonsrv::DBResultToJSON::setFormat(LightSpeed::ConstStrA name, FieldFormat fld) {
	for (LightSpeed::natural i = 0; i < rowDesc.length();i++)
		if (rowDesc[i].name == name) rowDesc(i).format = fld;
}

void jsonsrv::DBResultToJSON::setGroup(LightSpeed::ConstStrA name) {
	for (LightSpeed::natural i = 0; i < rowDesc.length();i++)
		if (rowDesc[i].name.head(name.length()) == name) {
			rowDesc(i).groupSep = name.length() - 1;
		}
}

LightSpeed::JSON::PNode jsonsrv::DBResultToJSON::getHeader() {
	using namespace LightSpeed;
	JSON::PNode obj = factory->newArray();

	for (natural i = 0; i < rowDesc.length(); i++) {
		obj->add(factory->newValue(rowDesc[i].name));
	}
	return obj;
}


LightSpeed::JSON::PNode jsonsrv::DBResultToJSON::getRow(const LightMySQL::Row& row) {
	using namespace LightSpeed;
	JSON::PNode obj = buildArray?factory->newArray():factory->newClass();
	natural c = row.size();
	for (natural i = 0; i < c; i++) {
		const RowItem &r = rowDesc[i];
		if (r.format == skip) continue;
		JSON::PNode nd;

		if (row[i].isNull()) {
			nd = factory->newNullNode();
		} else {
			switch (r.format) {
				case integer: nd = factory->newValue(row[i].as<LightSpeed::integer>());break;
				case datetime: nd = parseDateTime(*factory,row[i].as<ConstStrA>(),strDate);break;
				case floatnum: nd = factory->newValue(row[i].as<double>());break;
				case string: nd = factory->newValue(row[i].as<ConstStrA>());break;
				case set: nd = createArrayFromSet(*factory,row[i].as<ConstStrA>());break;
				case setWithCustomSep1: nd = createArrayFromSet(*factory,row[i].as<ConstStrA>(),customSep1);break;
				case setWithCustomSep2: nd = createArrayFromSet(*factory,row[i].as<ConstStrA>(),customSep2);break;
				case binary: nd = factory->newValue(base64_encode(row[i].as<ConstStrA>()));
				case jsonstr: {
					if (row[i].as<ConstStrA>().empty()) nd = factory->newNullNode();
					else {
						try {
							nd = factory->fromString(row[i].as<ConstStrA>());
						} catch (...) {
							nd = factory->newValue(row[i].as<ConstStrA>());
						}
					}
					break;
				}
				case boolean: {
					ConstStrA s = row[i].as<ConstStrA>();
					nd = factory->newValue(s != "false" && s != "FALSE" && s != "False" && s != "0");
				}
				default: continue;
			}
		}
		if (r.groupSep != naturalNull) {
			ConstStrA grname = r.name.head(r.groupSep);
			ConstStrA itemName = r.name.offset(r.groupSep+1);
			JSON::PNode e = obj->getVariable(grname);
			if (e == nil) {
				e = factory->newObject();
				obj->add(grname,e);
			}
			e->add(itemName,nd);
		} else {
			obj->add(r.name,nd);
		}

	}
	return obj;
}

ConstStrA dbDateTimeFormat("%Y-%m-%d %H:%M:%S");

LightSpeed::StringA dateTimeToDB(const LightSpeed::JSON::INode &nd) {
	using namespace LightSpeed;
	if (nd.getType() == JSON::ndString) {
		ConstStrA str = nd.getStringUtf8();
		if (str==ConstStrA("NOW")) {
			TimeStamp st = TimeStamp::now();
			return st.formatTime(dbDateTimeFormat);
		}
		return nd.getStringUtf8();
	} else if (nd.getType() == JSON::ndFloat || nd.getType() == JSON::ndInt) {
		TimeStamp st(nd.getFloat());
		return st.formatTime(dbDateTimeFormat);
	} else if (nd.getType() == JSON::ndNull) {
		return "0000-00-00 00:00:00";
	}
	return "0000-00-00 00:00:00";

}
LightSpeed::StringA flagsToDB(const LightSpeed::JSON::INode &nd) {
	using namespace LightSpeed;
	if (nd.getType() == JSON::ndString) return nd.getStringUtf8();
	AutoArray<char, SmallAlloc<256> > buff;
	ConstStrA comma(",");
	ConstStrA div = comma.head(0);
	for(natural i = 0; i < nd.getEntryCount(); i++) {
		buff.append(div);
		div = comma;
		buff.append(nd[i].getString());
	}
	return buff;

}

LightSpeed::natural dbDuplicateRecord(WriteTransaction &transaction, ConstStrA table, ConstStrA idField, LightSpeed::natural id) {
	LightMySQL::Result res = transaction("CREATE TEMPORARY TABLE tmp AS (SELECT * FROM %1 WHERE %2=%3);"
			"UPDATE tmp SET %2=NULL;"
			"INSERT INTO %1 SELECT * FROM tmp;"
			"DROP TABLE tmp").field(table).field(idField).arg(id).exec();
	res.nextResult();
	res.nextResult();
	return res.getInsertId();

}

LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode &nd) {
	using namespace LightSpeed;
	AutoArray<char, SmallAlloc<256> > buff;
	for (natural i = 0; i < nd.getEntryCount();i++) {
		ConstStrA fld = nd[i].getStringUtf8();
		if (fld.empty() || fld == ConstStrA('^') || fld.find('`') != naturalNull)
			  throw ErrorMessageException(THISLOCATION, "Unacceptable field name");
		if (i) buff.append(ConstStrA(", "));
		if (fld[0] == '^') {
			buff.add('`');buff.append(fld.offset(1));buff.add('`');
			buff.append(ConstStrA(" DESC"));
		} else {
			buff.add('`');buff.append(fld);buff.add('`');
			buff.append(ConstStrA(" ASC"));
		}
	}
	return ConstStrA(buff);
}

LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode *nd) {
	using namespace LightSpeed;
	if (nd == 0) return StringA();
	else return dbOrderFromJSON(*nd);
}

static ConstStrA findInAllowedSet(ConstStrA field, ConstStrA allowedSet) {
	using namespace LightSpeed;
	for (ConstStrA::SplitIterator iter = allowedSet.split(',');iter.hasItems();) {
		ConstStrA value = iter.getNext();
		natural eq = value.find('=');
		if (eq == naturalNull) {
			if (value == field) return value;
		} else {
			if (value.head(eq) == field) {
				return value.offset(eq+1);
			}
		}
	}
	return ConstStrA();
}

static void appendField(const LightSpeed::ConstStrA& trans,LightSpeed::AutoArray<char, LightSpeed::SmallAlloc<256> >& buff) {
	using namespace LightSpeed;
	ConstStrA::SplitIterator iter = trans.split('.');
	buff.add('`');
	buff.append(iter.getNext());
	buff.add('`');
	while (iter.hasItems()) {
		buff.add('.');
		buff.add('`');
		buff.append(iter.getNext());
		buff.add('`');
	}
}

LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode &nd, ConstStrA allowedSet) {
	using namespace LightSpeed;
	AutoArray<char, SmallAlloc<256> > buff;
	for (natural i = 0; i < nd.getEntryCount();i++) {
		ConstStrA fld = nd[i].getStringUtf8();
		if (fld.empty() || fld == ConstStrA('^') || fld.find('`') != naturalNull)
			  throw ErrorMessageException(THISLOCATION, "Unacceptable field name");
		if (i) buff.append(ConstStrA(", "));
		if (fld[0] == '^') {
			ConstStrA trans = findInAllowedSet(fld.offset(1),allowedSet);
			if (!trans.empty()) {
				appendField(trans, buff);
				buff.append(ConstStrA(" DESC"));
			}
		} else {
			ConstStrA trans = findInAllowedSet(fld,allowedSet);
			if (!trans.empty()) {
				appendField(trans, buff);
				buff.append(ConstStrA(" ASC"));
			}
		}
	}
	return ConstStrA(buff);
}

LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode *nd, ConstStrA allowedSet) {
	using namespace LightSpeed;
	if (nd == 0) return StringA();
	else return dbOrderFromJSON(*nd,allowedSet);
}
}


template void LightSpeed::AutoArrayT<jsonsrv::DBResultToJSON::RowItem,LightSpeed::SmallAlloc<16> >::clear();
