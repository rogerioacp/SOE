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
#ifndef ITUP_H
#define ITUP_H


//Oriignal postgres include, does not need to be modified.
#include <storage/itemptr.h>

#define INDEX_SIZE_MASK 0x1FFF

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

} IndexTupleData;				/* MORE DATA FOLLOWS AT END OF STRUCT */

typedef IndexTupleData *IndexTuple;


#define IndexTupleSize(itup)		((Size) ((itup)->t_info & INDEX_SIZE_MASK))
