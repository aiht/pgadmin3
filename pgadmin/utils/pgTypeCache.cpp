//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
// RCS-ID:      $Id$
// Copyright (C) 2002 - 2008, The pgAdmin Development Team
// This software is released under the Artistic Licence
//
// pgTypeCache.cpp - cache for data type names and details.
//
//////////////////////////////////////////////////////////////////////////

#include "pgAdmin3.h"

#include "utils/pgTypeCache.h"
#include "db/pgConn.h"
#include "utils/pgDefs.h"

// Some extra logging that can be turned on for debugging the cache, without making the rest of the log unreadable.
//#define TYPE_CACHE_EXTRA_LOG

#ifdef TYPE_CACHE_EXTRA_LOG
#define wxLogDebugExtra(msg) wxLogDebug(msg)
#else
#define wxLogDebugExtra(msg)
#endif


CachedTypeDetails::CachedTypeDetails()
	: typeClass(PGTYPCLASS_OTHER),
	typTypMod(0)
{
}

void pgTypeCache::SetInitial(const pgTypeCache *other)
{
	wxASSERT(other != NULL);
	wxLogDebug(wxString::Format(wxT("pgTypeCache: set initial: %d cached types"), other->types.size()));
	types = other->types;
	previousLimit = other->previousLimit;
}

wxString pgTypeCache::GetTypeName(OID oid)
{
	oidTypeMap::iterator it = iLoadTypeIfMissing(oid);
	wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: got basic name for oid %d: %s"), oid, (*it).second.basicName.c_str()));
	return (*it).second.basicName;
}

pgTypClass pgTypeCache::GetTypeClass(OID oid)
{
	oidTypeMap::iterator it = iLoadTypeIfMissing(oid);
	wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: got type class for oid %d: %d"), oid, (*it).second.typeClass));
	return (*it).second.typeClass;
}

wxString pgTypeCache::GetFullTypeName(OID oid, int typeMod)
{
	oidTypeMap::iterator it = iLoadTypeIfMissing(oid, &typeMod);
	wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: got full name for oid %d, typeMod %d: %s"), oid, typeMod, (*it).second.fullNames[typeMod].c_str()));
	return (*it).second.fullNames[typeMod];
}

wxString pgTypeCache::GetDefaultFullTypeName(OID oid)
{
	oidTypeMap::iterator it = iLoadTypeIfMissing(oid);
	wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: got default full name for oid %d: %s"), oid, (*it).second.fullNames[(*it).second.typTypMod].c_str()));
	return (*it).second.fullNames[(*it).second.typTypMod];
}

void pgTypeCache::PreCache(int limit /*= 0*/)
{
	if (previousLimit == 0 || limit > previousLimit)
	{
		iLoadTypes(limit, 0, NULL);
		previousLimit = limit;
		if (previousLimit == 0)
			previousLimit = types.size();
	}
}

pgTypClass pgTypeCache::iGetTypeClass(OID baseOid)
{
	switch (baseOid)
	{
		case PGOID_TYPE_BOOL:
			return PGTYPCLASS_BOOL;

		case PGOID_TYPE_INT8:
		case PGOID_TYPE_INT2:
		case PGOID_TYPE_INT4:
		case PGOID_TYPE_OID:
		case PGOID_TYPE_XID:
		case PGOID_TYPE_TID:
		case PGOID_TYPE_CID:
		case PGOID_TYPE_FLOAT4:
		case PGOID_TYPE_FLOAT8:
		case PGOID_TYPE_MONEY:
		case PGOID_TYPE_BIT:
		case PGOID_TYPE_NUMERIC:
			return PGTYPCLASS_NUMERIC;

		case PGOID_TYPE_BYTEA:
		case PGOID_TYPE_CHAR:
		case PGOID_TYPE_NAME:
		case PGOID_TYPE_TEXT:
		case PGOID_TYPE_VARCHAR:
			return PGTYPCLASS_STRING;

		case PGOID_TYPE_TIMESTAMP:
		case PGOID_TYPE_TIMESTAMPTZ:
		case PGOID_TYPE_TIME:
		case PGOID_TYPE_TIMETZ:
		case PGOID_TYPE_INTERVAL:
			return PGTYPCLASS_DATE;

		default:
			return PGTYPCLASS_OTHER;
	}
}

// Make sure that the requested OID and typmod combination is cached,
// and return an iterator to the OID record.
oidTypeMap::iterator pgTypeCache::iLoadTypeIfMissing(OID oid, int *typeMod /*= NULL*/)
{
	oidTypeMap::iterator it = types.find(oid);
	bool typeMissing = it == types.end();
	bool modMissing = !typeMissing && typeMod != NULL &&
		(*it).second.fullNames.find(*typeMod) == (*it).second.fullNames.end();
	if (typeMissing)
	{
		wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: cache miss for oid %d"), oid));
		iLoadTypes(0, oid, typeMod);
		it = types.find(oid);
	}
	else if (modMissing)
	{
		wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: partial cache hit for oid %d, typeMod %d"), oid, *typeMod));
		iLoadModName(it, *typeMod);
	}
	else
	{
		wxLogDebugExtra(wxString::Format(wxT("pgTypeCache: full cache hit for oid %d"), oid));
	}

	// Wait, what happens if we try to load an invalid OID? Bad things!
	// It shouldn't happen, but maybe if a type was deleted after we got a field of that type in our resultset?
	if (it == types.end())
	{
		wxLogWarning(wxString::Format(wxT("pgTypeCache: unable to load oid %d, returning dummy type info"), oid));
		// Make up something that will at least avoid crashes.
		CachedTypeDetails &currType = types[oid];

		currType.typTypMod = -1;
		currType.basicName = wxT("(unknown)");
		currType.fullNames[currType.typTypMod] = wxT("(unknown)");
		if (typeMod != NULL)
		{
			currType.fullNames[*typeMod] = wxT("(unknown)");
		}
		currType.typeClass = iGetTypeClass(oid);
		it = types.find(oid);
	}

	wxASSERT(it != types.end());
	return it;
}

// Load one or many type records, depending on whether oid is valid.
// If one record is loaded, typeMod can point to a custom typmod value.
// If many are loaded, the result row count can be limited by limit.
void pgTypeCache::iLoadTypes(int limit, OID oid, int *typeMod)
{
	bool useLimit = limit > 0;
	bool useOid = oid > 0;
	bool useMod = useOid && typeMod != NULL;

	const wxChar *sqlCommonFields = wxT("typtypmod,typbasetype,")
			wxT("format_type(oid,NULL) as basic,")
			wxT("format_type(oid,typtypmod) as def");
	const wxChar *sqlFrom = wxT(" FROM pg_type");

	wxString sql, tmp;
	if (useOid)		// load a single type record
	{
		sql = wxT("SELECT ");
		sql += sqlCommonFields;
		if (useMod)
		{
			tmp.Printf(wxT(",format_type(oid,%d) as bymod"), *typeMod);
			sql += tmp;
			wxLogDebug(wxString::Format(wxT("pgTypeCache: loading oid %d with typeMod %d"), oid, *typeMod));
		}
		else
		{
			wxLogDebug(wxString::Format(wxT("pgTypeCache: loading oid %d"), oid));
		}

		sql += sqlFrom;
		tmp.Printf(wxT(" WHERE oid=%d"), oid);
		sql += tmp;
	}
	else
	{
		sql = wxT("SELECT oid,");
		sql += sqlCommonFields;
		sql += sqlFrom;
		if (useLimit)
		{
			tmp.Printf(wxT(" LIMIT %d"), limit);
			sql += tmp;
			wxLogDebug(wxString::Format(wxT("pgTypeCache: loading up to %d types in a batch"), limit));
		}
		else
			wxLogDebug(wxString::Format(wxT("pgTypeCache: loading entire pg_type table in a batch")));
	}

	pgSet *result = conn->ExecuteSet(sql);
	if (result)
	{
		while (!result->Eof())
		{
			OID currOid = useOid? oid : result->GetOid(wxT("oid"));
			CachedTypeDetails &currType = types[currOid];

			currType.typTypMod = result->GetLong(wxT("typtypmod"));
			currType.basicName = result->GetVal(wxT("basic"));
			currType.fullNames[currType.typTypMod] = result->GetVal(wxT("def"));
			if (useMod)
			{
				currType.fullNames[*typeMod] = result->GetVal(wxT("bymod"));
			}
			OID baseType = result->GetOid(wxT("typbasetype"));
			currType.typeClass = iGetTypeClass(baseType == 0? currOid : baseType);

			result->MoveNext();
		}
		delete result;
	}
	else
	{
		wxLogError(wxString::Format(wxT("pgTypeCache: failed to load type details")));
	}
}

// Load a typmod-modified name for a type that is already cached.
void pgTypeCache::iLoadModName(oidTypeMap::iterator existingType, int typeMod)
{
	wxASSERT(existingType != types.end());
	wxLogDebug(wxString::Format(wxT("pgTypeCache: loading full name for oid %d typmod %d"), (*existingType).first, typeMod));
	wxString szSQL;
	szSQL.Printf(wxT("SELECT format_type(%d,%d)"), (*existingType).first, typeMod);
	(*existingType).second.fullNames[typeMod] = conn->ExecuteScalar(szSQL);
}
