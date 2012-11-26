//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
//
// Copyright (C) 2002 - 2012, The pgAdmin Development Team
// This software is released under the PostgreSQL Licence
//
// pgDatatype.cpp - PostgreSQL Datatypes
//
//////////////////////////////////////////////////////////////////////////


#include <wx/wx.h>

// App headers
#include "pgAdmin3.h"
#include "utils/misc.h"
#include "schema/pgDatatype.h"
#include "schema/pgDatabase.h"
#include "utils/pgDefs.h"


pgDatatype::pgDatatype()
{
	needSchema = false;
	oid = 0;
	typmod = -1;
	typtype = '\0';
}

pgDatatype::pgDatatype(OID o, const wxString &nsp, const wxString &typname, char ttype, bool isDup)
{
	needSchema = isDup;
	schema = nsp;
	oid = o;
	typmod = -1;
	typtype = ttype;

	init(typname, 0);
}

pgDatatype::pgDatatype(const wxString &nsp, const wxString &typname, bool isDup, long numdims, long mod)
{
	needSchema = isDup;
	schema = nsp;
	oid = 0;
	typtype = '\0';
	typmod = mod;

	init(typname, numdims);
}

void pgDatatype::init(const wxString &typname, long numdims)
{
	// Above 7.4, format_type also sends the schema name if it's not included
	// in the search_path, so we need to skip it in the typname
	if (typname.Contains(schema + wxT("\".")))
		name = typname.Mid(schema.Len() + 3); // "+2" because of the two double quotes
	else if (typname.Contains(schema + wxT(".")))
		name = typname.Mid(schema.Len() + 1);
	else
		name = typname;

	if (name.StartsWith(wxT("_")))
	{
		if (!numdims)
			numdims = 1;
		name = name.Mid(1);
	}
	if (name.Right(2) == wxT("[]"))
	{
		if (!numdims)
			numdims = 1;
		name = name.Left(name.Len() - 2);
	}

	if (name.StartsWith(wxT("\"")) && name.EndsWith(wxT("\"")))
		name = name.Mid(1, name.Len() - 2);

	if (numdims > 0)
	{
		while (numdims--)
			array += wxT("[]");
	}
}

wxString pgDatatype::GetLengthText(long mod) const
{
	wxString lenText;
	long len=0, prec=0;
	ParseTypeMod(mod, lenText, len, prec);
	return lenText;
}

long pgDatatype::Length() const
{
	return Length(typmod);
}

long pgDatatype::Precision() const
{
	return Precision(typmod);
}

long pgDatatype::Length(long mod) const
{
	wxString lenText;
	long len=0, prec=0;
	ParseTypeMod(mod, lenText, len, prec);
	return len;
}

long pgDatatype::Precision(long mod) const
{
	wxString lenText;
	long len=0, prec=0;
	ParseTypeMod(mod, lenText, len, prec);
	return prec;
}

void pgDatatype::ParseTypeMod(long mod, wxString &lengthText, long &len, long &prec) const
{
	if (mod != -1)
	{
		lengthText = wxT("(");
		if (name == wxT("numeric"))
		{
			len = (mod - 4L) >> 16L;
			prec = (mod - 4) & 0xffff;
			lengthText += NumToStr(len);
			if (prec)
				lengthText += wxT(",") + NumToStr(prec);
		}
		else if (name == wxT("time") || name == wxT("timetz")
		         || name == wxT("time without time zone") || name == wxT("time with time zone")
		         || name == wxT("timestamp") || name == wxT("timestamptz")
		         || name == wxT("timestamp without time zone") || name == wxT("timestamp with time zone")
		         || name == wxT("bit") || name == wxT("bit varying") || name == wxT("varbit"))
		{
			prec = 0;
			len = mod;
			lengthText += NumToStr(len);
		}
		else if (name == wxT("interval"))
		{
			prec = 0;
			len = (mod & 0xffff);
			lengthText += NumToStr(len);
		}
		else if (name == wxT("date"))
		{
			len = prec = 0;
			lengthText.Clear();
		}
		else
		{
			prec = 0;
			len = mod - 4L;
			lengthText += NumToStr(len);
		}

		if (lengthText.Length() > 0)
			lengthText += wxT(")");
	}
	else
	{
		lengthText.Clear();
		len = prec = 0;
	}
}

// Return the full name of the type, with default dimension and array qualifiers
wxString pgDatatype::FullName() const
{
	return FullName(typmod);
}

// Return the full name of the type, with dimension and array qualifiers according to typmod
wxString pgDatatype::FullName(long mod) const
{
	wxString length = GetLengthText(mod);

	if (name == wxT("char") && schema == wxT("pg_catalog"))
		return wxT("\"char\"") + array;
	else if (name == wxT("time with time zone"))
		return wxT("time") + length + wxT(" with time zone") + array;
	else if (name == wxT("time without time zone"))
		return wxT("time") + length + wxT(" without time zone") + array;
	else if (name == wxT("timestamp with time zone"))
		return wxT("timestamp") + length + wxT(" with time zone") + array;
	else if (name == wxT("timestamp without time zone"))
		return wxT("timestamp") + length + wxT(" without time zone") + array;
	else
		return name + length + array;
}

// Return the quoted full name of the type, with default dimension and array qualifiers
wxString pgDatatype::QuotedFullName() const
{
	return QuotedFullName(typmod);
}

// Return the quoted full name of the type, with dimension and array qualifiers according to typmod
wxString pgDatatype::QuotedFullName(long mod) const
{
	wxString length = GetLengthText(mod);

	if (name == wxT("char") && schema == wxT("pg_catalog"))
		return wxT("\"char\"") + array;
	else if (name == wxT("time with time zone"))
		return wxT("time") + length + wxT(" with time zone") + array;
	else if (name == wxT("time without time zone"))
		return wxT("time") + length + wxT(" without time zone") + array;
	else if (name == wxT("timestamp with time zone"))
		return wxT("timestamp") + length + wxT(" with time zone") + array;
	else if (name == wxT("timestamp without time zone"))
		return wxT("timestamp") + length + wxT(" without time zone") + array;
	else
		return qtTypeIdent(name) + length + array;
}

wxString pgDatatype::GetSchemaPrefix(pgDatabase *db) const
{
	if (schema.IsEmpty() || (!db && schema == wxT("pg_catalog")))
		return wxEmptyString;

	if (needSchema)
		return schema + wxT(".");

	return db->GetSchemaPrefix(schema);
}


wxString pgDatatype::GetQuotedSchemaPrefix(pgDatabase *db) const
{
	wxString str = GetSchemaPrefix(db);
	if (!str.IsEmpty())
		return qtIdent(str.Left(str.Length() - 1)) + wxT(".");
	return str;
}


long pgDatatype::GetTypmod(const wxString &name, const wxString &len, const wxString &prec)
{
	if (len.IsEmpty())
		return -1;
	if (name == wxT("numeric"))
	{
		return (((long)StrToLong(len) << 16) + StrToLong(prec)) + 4;
	}
	else if (name == wxT("time") || name == wxT("timetz")
	         || name == wxT("time without time zone") || name == wxT("time with time zone")
	         || name == wxT("timestamp") || name == wxT("timestamptz")
	         || name == wxT("timestamp without time zone") || name == wxT("timestamp with time zone")
	         || name == wxT("interval")  || name == wxT("bit") || name == wxT("bit varying") || name == wxT("varbit"))
	{
		return StrToLong(len);
	}
	else
	{
		return StrToLong(len) + 4;
	}
}


DatatypeReader::DatatypeReader(pgDatabase *db, const wxString &condition, bool addSerials)
{
	init(db->GetConnection(), condition, addSerials);
}

DatatypeReader::DatatypeReader(pgConn *conn, const wxString &condition, bool addSerials)
{
	init(conn, condition, addSerials);
}


void DatatypeReader::temp_init(pgConn *conn, bool withDomains, bool addSerials)
{
	wxString condition = wxT("typisdefined AND typtype ");
	// We don't get pseudotypes here
	if (withDomains)
		condition += wxT("IN ('b', 'c', 'd', 'e', 'r')");
	else
		condition += wxT("IN ('b', 'c', 'e', 'r')");

	condition += wxT("AND NOT EXISTS (select 1 from pg_class where relnamespace=typnamespace and relname = typname and relkind != 'c') AND (typname not like '_%' OR NOT EXISTS (select 1 from pg_class where relnamespace=typnamespace and relname = substring(typname from 2)::name and relkind != 'c')) ");

	if (!settings->GetShowSystemObjects())
		condition += wxT(" AND nsp.nspname != 'information_schema'");
	init(conn, condition, addSerials);
}

DatatypeReader::DatatypeReader(pgDatabase *db, bool withDomains, bool addSerials)
{
	temp_init(db->GetConnection(), withDomains, addSerials);
}

DatatypeReader::DatatypeReader(pgConn *conn, bool withDomains, bool addSerials)
{
	temp_init(conn, withDomains, addSerials);
}


DatatypeReader::DatatypeReader(pgDatabase *db, OID oid)
{
	wxString condition;
	condition.Printf(wxT("oid = %d"), oid);

	init(db->GetConnection(), condition, false, true);
}

DatatypeReader::DatatypeReader(pgConn *conn, OID oid)
{
	wxString condition;
	condition.Printf(wxT("oid = %d"), oid);

	init(conn, condition, false, true);
}

void DatatypeReader::init(pgConn *conn, const wxString &condition, bool addSerials, bool allowUnknown)
{
	connection = conn;
	wxString sql = wxT("SELECT * FROM (SELECT format_type(t.oid,NULL) AS typname, typtype, t.oid as oid, nspname,\n")
	               wxT("       (SELECT COUNT(1) FROM pg_type t2 WHERE t2.typname = t.typname) > 1 AS isdup\n")
	               wxT("  FROM pg_type t\n")
	               wxT("  JOIN pg_namespace nsp ON typnamespace=nsp.oid\n")
	               wxT(" WHERE ");
	if (!allowUnknown)
	{
		sql += wxT("(NOT (typname = 'unknown' AND nspname = 'pg_catalog')) AND ");
	}

	sql += condition + wxT("\n");

	if (addSerials)
	{
		sql += wxT(" UNION SELECT 'bigserial', 'b', 0, 'pg_catalog', false\n");
		sql += wxT(" UNION SELECT 'serial', 'b', 0, 'pg_catalog', false\n");
	}

	sql += wxT("  ) AS dummy ORDER BY nspname <> 'pg_catalog', nspname <> 'public', nspname, 1");

	set = connection->ExecuteSet(sql);
}


bool DatatypeReader::IsDomain() const
{
	return set->GetVal(wxT("typtype")) == 'd';
}


// Not used, and saves two columns being read.
// If they are needed later, put "CASE WHEN typelem > 0 THEN typelem ELSE t.oid END as elemoid" and "typlen" back in the select list.
//bool DatatypeReader::IsVarlen() const
//{
//	return set->GetLong(wxT("typlen")) == -1;
//}
//
//
//bool DatatypeReader::MaySpecifyLength() const
//{
//	if (IsDomain())
//		return false;
//
//	switch ((long)set->GetOid(wxT("elemoid")))
//	{
//		case PGOID_TYPE_BIT:
//		case PGOID_TYPE_CHAR:
//		case PGOID_TYPE_VARCHAR:
//		case PGOID_TYPE_NUMERIC:
//			return true;
//		default:
//			return false;
//	}
//}
//
//
//bool DatatypeReader::MaySpecifyPrecision() const
//{
//	if (IsDomain())
//		return false;
//
//	switch ((long)set->GetOid(wxT("elemoid")))
//	{
//		case PGOID_TYPE_NUMERIC:
//			return true;
//		default:
//			return false;
//	}
//}


pgDatatype DatatypeReader::GetDatatype() const
{
	wxString tt = set->GetVal(wxT("typtype"));
	char ttype = tt.IsEmpty() ? '\0' : tt[0];
	return pgDatatype(GetOid(), GetSchema(), GetTypename(), ttype, set->GetBool(wxT("isdup")));
}


wxString DatatypeReader::GetTypename() const
{
	return set->GetVal(wxT("typname"));
}


wxString DatatypeReader::GetSchema() const
{
	return set->GetVal(wxT("nspname"));
}


wxString DatatypeReader::GetOidStr() const
{
	return set->GetVal(wxT("oid"));
}


OID DatatypeReader::GetOid() const
{
	return set->GetOid(wxT("oid"));
}


//--------------------------------------------------------------------------------------------------



void pgDatatypeCache::SetInitial(const pgDatatypeCache *other)
{
	wxASSERT(other != NULL);
	wxLogDebug(wxString::Format(wxT("pgDatatypeCache: set initial: %d cached types"), other->types.size()));
	types = other->types;
}

const pgDatatype *pgDatatypeCache::GetDatatype(OID oid) const
{
	if (types.empty())
	{
		DatatypeReader dr(conn, false, false);
		while (dr.HasMore())
		{
			pgDatatype d = dr.GetDatatype();
			types[d.Oid()] = d;
			dr.MoveNext();
		}
		wxLogDebug(wxString::Format(wxT("pgDatatypeCache: first load, %d cached types"), types.size()));
	}

	oidDatatypeMap::iterator it = types.find(oid);
	if (it == types.end())
	{
		DatatypeReader dr(conn, oid);
		while (dr.HasMore())
		{
			pgDatatype d = dr.GetDatatype();
			types[d.Oid()] = d;
			wxASSERT(oid == d.Oid());
			dr.MoveNext();
		}
		it = types.find(oid);
		wxLogDebug(wxString::Format(wxT("pgDatatypeCache: loaded single missing record, oid %d"), oid));
	}
	wxASSERT(it != types.end());
	return &(*it).second;
}
