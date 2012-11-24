//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2012, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// pgTypeCache.h - cache for data type names and details.
//
//////////////////////////////////////////////////////////////////////////

#ifndef PGTYPECACHE_H
#define PGTYPECACHE_H

// wxWindows headers
#include <wx/wx.h>
#include <wx/hashmap.h>

// PostgreSQL headers
#include <libpq-fe.h>

#include "utils/misc.h"

class pgConn;

WX_DECLARE_HASH_MAP(int, wxString, wxIntegerHash, wxIntegerEqual, typmodStringMap);

class CachedTypeDetails
{
public:
	CachedTypeDetails();

	pgTypClass typeClass;   // type class eg. numeric, string etc.
	wxString basicName;      // format_type(oid, NULL)
	int typTypMod;           // pg_type.typtypmod
	typmodStringMap fullNames;        // map of typMod -> format_type(oid, typMod)
};

WX_DECLARE_HASH_MAP(OID, CachedTypeDetails, wxIntegerHash, wxIntegerEqual, oidTypeMap);

class pgTypeCache
{
public:
	pgTypeCache(pgConn *cn) : conn(cn), previousLimit(0) {}

	// Pre-fill the cache with some cached data from another connection's cache.
	void SetInitial(const pgTypeCache *other);

	// Get the short type name from 'format_type(oid, NULL)'
	wxString GetTypeName(OID oid);
	// Get the full type name from 'format_type(oid, typeMod)'
	wxString GetFullTypeName(OID oid, int typeMod = -1);
	// Get the type class eg. numeric, string etc.
	pgTypClass GetTypeClass(OID oid);

	// Get the full type name from 'format_type(oid, pg_type.typtypmod)'
	// TODO: Check if this should still be used. My old patch used it in three places:
	// edbPackageFunctionFactory::AppendFunctions - now uses 'format_type(oid, NULL)'
	// pgAggregateFactory::CreateObjects - still uses typtypmod.
	// pgFunctionFactory::AppendFunctions - now uses 'format_type(oid, NULL)'
	wxString GetDefaultFullTypeName(OID oid);

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
	pgTypeCache(const pgTypeCache &rhs);
	const pgTypeCache &operator=(const pgTypeCache &rhs);

	oidTypeMap::iterator iLoadTypeIfMissing(OID oid, int *typeMod = NULL);
	pgTypClass iGetTypeClass(OID baseOid);
	void iLoadTypes(int limit, OID oid, int *typeMod = NULL);
	void iLoadModName(oidTypeMap::iterator existingType, int typeMod);

	pgConn *conn;
	oidTypeMap types;
	int previousLimit;
};

#endif
