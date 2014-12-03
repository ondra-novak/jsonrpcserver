/*
 * db.h
 *
 *  Created on: Sep 26, 2012
 *      Author: ondra
 */

#ifndef SNAPYTAP_DB_H_
#define SNAPYTAP_DB_H_
#include "lightmysql/transaction.h"
#include "lightmysql/row.h"
#include "lightspeed/utils/json.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/memory/smallAlloc.h"
#include "lightspeed/base/exceptions/genexcept.h"
#include "lightspeed/base/timestamp.h"


namespace jsonsrv {

typedef LightMySQL::Transaction ReadTransaction;
typedef LightMySQL::Transaction WriteTransaction;

LightSpeed::JSON::PNode dbResultToJsonObject(LightSpeed::JSON::IFactory &factory, LightMySQL::Row &row, bool strDate = false);
LightSpeed::JSON::PNode createArrayFromSet(LightSpeed::JSON::IFactory &factory, ConstStrA text, char sep = ',');
LightSpeed::TimeStamp parseDateTime(ConstStrA text);
LightSpeed::JSON::PNode parseDateTime(LightSpeed::JSON::IFactory &factory, ConstStrA text, bool strDate);
LightSpeed::StringA dateTimeToDB(const LightSpeed::JSON::INode &nd);
LightSpeed::StringA flagsToDB(const LightSpeed::JSON::INode &nd);
LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode &nd);
LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode *nd);
LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode &nd, ConstStrA allowedSet);
LightSpeed::StringA dbOrderFromJSON(const LightSpeed::JSON::INode *nd, ConstStrA allowedSet);
LightSpeed::natural dbDuplicateRecord(WriteTransaction &transaction, ConstStrA table, ConstStrA idField, LightSpeed::natural id);


class DBResultToJSON {
public:


	DBResultToJSON(LightSpeed::JSON::IFactory *factory, const LightMySQL::Result &result, bool strDate);
	DBResultToJSON(LightSpeed::JSON::IFactory *factory, const LightMySQL::Result &result, bool strDate, bool noInit, bool buildArray);

	enum FieldFormat {
		skip,
		integer,
		floatnum,
		datetime,
		string,
		set,
		setWithCustomSep1,
		setWithCustomSep2,
		binary,
		boolean,
		jsonstr
	};

	void setFormat(LightSpeed::natural index, FieldFormat fld);
	void setFormat(LightSpeed::ConstStrA name, FieldFormat fld);
	void rename(LightSpeed::natural index, LightSpeed::ConstStrA newName);
	void rename(LightSpeed::ConstStrA name, LightSpeed::ConstStrA newName);
	void setGroup(LightSpeed::ConstStrA name);
	LightSpeed::JSON::PNode getRow(const LightMySQL::Row &row);
	LightSpeed::JSON::PNode operator()(const LightMySQL::Row &row) {return getRow(row);}
	void setCustomSet1(char x) {customSep1 = x;}
	void setCustomSet2(char x) {customSep2 = x;}
	LightSpeed::JSON::PNode getHeader();

protected:
	struct RowItem {
		LightSpeed::ConstStrA name;
		FieldFormat format;
		LightSpeed::natural groupSep;
		RowItem (LightSpeed::ConstStrA name, FieldFormat format):name(name),format(format),groupSep(LightSpeed::naturalNull) {}
		RowItem (LightSpeed::ConstStrA name, FieldFormat format,LightSpeed::natural groupSep):name(name),format(format),groupSep(groupSep) {}
	};

	typedef LightSpeed::AutoArray<RowItem, LightSpeed::SmallAlloc<16> > RowDesc;
	LightSpeed::JSON::IFactory *factory;
	const LightMySQL::Result &result;
	bool strDate;
	bool buildArray;
	RowDesc rowDesc;
	char customSep1;
	char customSep2;

private:
	void init(LightSpeed::natural count, const LightMySQL::Result& result, bool hideAl);
};

extern const char *message_InvalidDateTimeFormat;

typedef LightSpeed::GenException1<message_InvalidDateTimeFormat, ConstStrA> InvalidDateTimeFormat;

}



#endif /* SNAPYTAP_DB_H_ */
