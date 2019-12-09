/*-------------------------------------------------------------------------
 *
 * soe_tupdesc.h
 * Bare bones copy of OSTGRES tuple descriptor definitions for
 * enclave execution
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/tupdesc.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SOE_TUPDESC_H
#define SOE_TUPDESC_H


#include "soe_c.h"
#include "catalog/soe_pg_attribute.h"

#include "access/soe_attnum.h"
#include "access/soe_tupdesc_details.h"


typedef struct attrDefault
{
	AttrNumber	adnum;
	char	   *adbin;			/* nodeToString representation of expr */
} AttrDefault;

typedef struct attrMissing *MissingPtr;

typedef struct constrCheck
{
	char	   *ccname;
	char	   *ccbin;			/* nodeToString representation of expr */
	bool		ccvalid;
	bool		ccnoinherit;	/* this is a non-inheritable constraint */
} ConstrCheck;

/* This structure contains constraints of a tuple */
typedef struct tupleConstr
{
	AttrDefault *defval;		/* array */
	ConstrCheck *check;			/* array */
	MissingPtr	missing;		/* missing attributes values, NULL if none */
	uint16		num_defval;
	uint16		num_check;
	bool		has_not_null;
} TupleConstr;

/*
 * This struct is passed around within the backend to describe the structure
 * of tuples.  For tuples coming from on-disk relations, the information is
 * collected from the pg_attribute, pg_attrdef, and pg_constraint catalogs.
 * Transient row types (such as the result of a join query) have anonymous
 * TupleDesc structs that generally omit any constraint info; therefore the
 * structure is designed to let the constraints be omitted efficiently.
 *
 * Note that only user attributes, not system attributes, are mentioned in
 * TupleDesc; with the exception that tdhasoid indicates if OID is present.
 *
 * If the tupdesc is known to correspond to a named rowtype (such as a table's
 * rowtype) then tdtypeid identifies that type and tdtypmod is -1.  Otherwise
 * tdtypeid is RECORDOID, and tdtypmod can be either -1 for a fully anonymous
 * row type, or a value >= 0 to allow the rowtype to be looked up in the
 * typcache.c type cache.
 *
 * Note that tdtypeid is never the OID of a domain over composite, even if
 * we are dealing with values that are known (at some higher level) to be of
 * a domain-over-composite type.  This is because tdtypeid/tdtypmod need to
 * match up with the type labeling of composite Datums, and those are never
 * explicitly marked as being of a domain type, either.
 *
 * Tuple descriptors that live in caches (relcache or typcache, at present)
 * are reference-counted: they can be deleted when their reference count goes
 * to zero.  Tuple descriptors created by the executor need no reference
 * counting, however: they are simply created in the appropriate memory
 * context and go away when the context is freed.  We set the tdrefcount
 * field of such a descriptor to -1, while reference-counted descriptors
 * always have tdrefcount >= 0.
 */
//typedef struct tupleDesc
//{
//	int			natts;			/* number of attributes in the tuple */
//	Oid			tdtypeid;		/* composite type ID for tuple type */
//	int32		tdtypmod;		/* typmod for tuple type */
//	bool		tdhasoid;		/* tuple has oid attribute in its header */
//	int			tdrefcount;		/* reference count, or -1 if not counting */
//	TupleConstr *constr;		/* constraints, or NULL if none */
	/* attrs[N] is the description of Attribute Number N+1 */
//	FormData_pg_attribute attrs[FLEXIBLE_ARRAY_MEMBER];
//}		   *TupleDesc;

typedef struct tupleDesc
{
	int			natts;			/* number of attributes in the tuple */
	//bool 		isnbtree;
	FormData_pg_attribute* attrs;
}	 *TupleDesc;

/* Accessor for the i'th attribute of tupdesc. */
#define TupleDescAttr_s(tupdesc, i) (&(tupdesc)->attrs[(i)])

#endif							/* TUPDESC_H */
