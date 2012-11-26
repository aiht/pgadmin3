//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2012, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// pgDatatype.h - PostgreSQL Datatypes
//
//////////////////////////////////////////////////////////////////////////

#ifndef __DATATYPE_INC
#define __DATATYPE_INC

#include <wx/wx.h>

// App headers

#include "db/pgSet.h"
#include "db/pgConn.h"

class pgDatabase;

class pgDatatype
{
public:
	pgDatatype();

	// For storing the type definitions loaded from the pg_type table.
	pgDatatype(OID oid, const wxString &nsp, const wxString &typname, char typtype, bool isduplicate);

	// For representing the type of a specific column of a specific table.
	pgDatatype(const wxString &nsp, const wxString &typname, bool isduplicate, long numdims, long typmod);

	wxString Name() const
	{
		return name;
	}
	OID Oid() const
	{
		return oid;
	}
	int GetTyptype() const
	{
		return typtype;
	}
	wxString Array() const
	{
		return array;
	}

	wxString FullName() const;
	wxString QuotedFullName() const;

	wxString FullName(long typmod) const;
	wxString QuotedFullName(long typmod) const;

	wxString GetSchemaPrefix(pgDatabase *db) const;
	wxString GetQuotedSchemaPrefix(pgDatabase *db) const;

	long Length() const;
	long Precision() const;

	long Length(long typmod) const;
	long Precision(long typmod) const;

	long GetTypmod() const
	{
		return typmod;
	}
	static long GetTypmod(const wxString &name, const wxString &len, const wxString &prec);

private:
	void init(const wxString &typname, long numdims);

	wxString GetLengthText(long typmod) const;
	void ParseTypeMod(long typmod, wxString &lengthText, long &length, long &precision) const;

	wxString schema;
	wxString name;
	wxString array;
	long typmod;	// Only stored for a type instance (second constructor).
	OID oid;
	char typtype;
	bool needSchema;
};


class DatatypeReader
{
public:
	DatatypeReader(pgDatabase *db, bool withDomains = true, bool addSerials = false);
	DatatypeReader(pgDatabase *db, const wxString &condition, bool addSerials = false);
	DatatypeReader(pgDatabase *db, OID oid);

	DatatypeReader(pgConn *conn, bool withDomains = true, bool addSerials = false);
	DatatypeReader(pgConn *conn, const wxString &condition, bool addSerials = false);
	DatatypeReader(pgConn *conn, OID oid);

	~DatatypeReader()
	{
		if (set) delete set;
	}
	bool HasMore() const
	{
		return set && !set->Eof();
	}
	void MoveNext()
	{
		if (set)  set->MoveNext();
	}

	bool IsDomain() const;
	bool IsVarlen() const;
	bool MaySpecifyLength() const;
	bool MaySpecifyPrecision() const;
	pgDatatype GetDatatype() const;
	wxString GetTypename() const;
	wxString GetSchema() const;
	wxString GetOidStr() const;
	OID GetOid() const;

private:
	pgSet *set;
	pgConn *connection;
	void init(pgConn *conn, const wxString &condition, bool addSerials = false, bool allowUnknown = false);
	void temp_init(pgConn *conn, bool withDomains, bool addSerials);
};



WX_DECLARE_HASH_MAP(OID, pgDatatype, wxIntegerHash, wxIntegerEqual, oidDatatypeMap);
WX_DEFINE_ARRAY(const pgDatatype *, dataTypes);

class pgDatatypeCache
{
public:
	pgDatatypeCache(pgConn *cn) : conn(cn) {}

	// Pre-fill the cache with some data from another connection's cache.
	void SetInitial(const pgDatatypeCache *other);

	const pgDatatype *GetDatatype(OID oid) const;

	// Pre-fill the cache with all types in the database, or with only 'limit' types if it is provided.
	void PreCache(int limit = 0);

	// Make sure the two-way link to pgConn is correct.
	void assertConnMatchLink( pgConn *test )
	{
		wxASSERT(conn != NULL);
		wxASSERT(conn == test);
	}

private:
	// This class can not be copied.
	pgDatatypeCache(const pgDatatypeCache &rhs);
	const pgDatatypeCache &operator=(const pgDatatypeCache &rhs);

	oidDatatypeMap::iterator iLoadTypeIfMissing(OID oid);
	pgTypClass iGetTypeClass(OID baseOid);
	void iLoadTypes(int limit, OID oid = 0);

	pgConn *conn;
	mutable oidDatatypeMap types;
};

#endif
