/*-------------------------------------------------------------------------
 *
 * soeitup.h
 *	  Copy of POSTGRES index tuple definitions.
 *    Only contains the relevant itup definitions for an enclave execution.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/itup.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SOE_ITUP_H
#define SOE_ITUP_H


#include "soe_c.h"

/* Oriignal postgres include, does not need to be modified. */
#include "storage/soe_itemptr.h"

#include "access/soe_tupdesc.h"


typedef struct IndexAttributeBitMapData
{
	bits8		bits[(INDEX_MAX_KEYS + 8 - 1) / 8];
}			IndexAttributeBitMapData;

typedef IndexAttributeBitMapData * IndexAttributeBitMap;


#define INDEX_SIZE_MASK 0x1FFF
#define INDEX_AM_RESERVED_BIT 0x2000	/* reserved for index-AM specific
										 * usage */

#define INDEX_VAR_MASK	0x4000
#define INDEX_NULL_MASK 0x8000



/*
 * Index tuple header structure
 *
 * All index tuples start with IndexTupleData.  If the HasNulls bit is set,
 * this is followed by an IndexAttributeBitMapData.  The index attribute
 * values follow, beginning at a MAXALIGN boundary.
 *
 * Note that the space allocated for the bitmap does not vary with the number
 * of attributes; that is because we don't have room to store the number of
 * attributes in the header.  Given the MAXALIGN constraint there's no space
 * savings to be had anyway, for usual values of INDEX_MAX_KEYS.
 */

typedef struct IndexTupleData
{
	ItemPointerData t_tid;		/* reference TID to heap tuple */

	/* ---------------
	 * t_info is laid out in the following fashion:
	 *
	 * 15th (high) bit: has nulls
	 * 14th bit: has var-width attributes
	 * 13th bit: AM-defined meaning
	 * 12-0 bit: size of tuple
	 * ---------------
	 */

	unsigned short t_info;		/* various info about tuple */

}			IndexTupleData;		/* MORE DATA FOLLOWS AT END OF STRUCT */

typedef IndexTupleData * IndexTuple;


/*
 * MaxIndexTuplesPerPage is an upper bound on the number of tuples that can
 * fit on one index page.  An index tuple must have either data or a null
 * bitmap, so we can safely assume it's at least 1 byte bigger than a bare
 * IndexTupleData struct.  We arrive at the divisor because each tuple
 * must be maxaligned, and it must have an associated item pointer.
 *
 * To be index-type-independent, this does not account for any special space
 * on the page, and is thus conservative.
 *
 * Note: in btree non-leaf pages, the first tuple has no key (it's implicitly
 * minus infinity), thus breaking the "at least 1 byte bigger" assumption.
 * On such a page, N tuples could take one MAXALIGN quantum less space than
 * estimated here, seemingly allowing one more tuple than estimated here.
 * But such a page always has at least MAXALIGN special space, so we're safe.
 */
#define MaxIndexTuplesPerPage	\
	((int) ((BLCKSZ - SizeOfPageHeaderData) / \
			(MAXALIGN_s(sizeof(IndexTupleData) + 1) + sizeof(ItemIdData))))

#define IndexTupleSize_s(itup)		((Size) ((itup)->t_info & INDEX_SIZE_MASK))

/*
 * Takes an infomask as argument (primarily because this needs to be usable
 * at index_form_tuple time so enough space is allocated).
 */
#define IndexInfoFindDataOffset_s(t_info) \
( \
	(!((t_info) & INDEX_NULL_MASK)) ? \
	( \
		(Size)MAXALIGN_s(sizeof(IndexTupleData)) \
	) \
	: \
	( \
		(Size)MAXALIGN_s(sizeof(IndexTupleData) + sizeof(IndexAttributeBitMapData)) \
	) \
)

/* ----------------
 *		index_getattr_s
 *		Bare bones copy of index_getattr defnied in postgres that assumes
 *      no nulls and that the scan key data is the first attrib
 *		This gets called many times, so we macro the cacheable and NULL
 *		lookups, and call nocache_index_getattr() for the rest.
 *
 * ----------------
 */
#define index_getattr_s(tup) \
	((char *) (tup) + IndexInfoFindDataOffset_s((tup)->t_info))



/* routines in indextuple.c */
extern IndexTuple index_form_tuple_s(TupleDesc tupleDescriptor,
									 Datum * values, bool *isnull);

#endif
