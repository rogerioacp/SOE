/*-------------------------------------------------------------------------
 *
 * soe.h
 *	  The public API for the secure operator evaluator (SOE). This API defines 
 *    the functionalities available to securely process postgres queries.
 *
 * Copyright (c) 2018-2019, HASLab
 *
 * src/include/soe.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_H
#define SOE_H

#include <oram/oram.h>
#include "soe_c.h"
#include "access/soe_htup.h"
#include <oram/ofile.h>


//#include "access/attnum.h"

/*
 * user defined attribute numbers start at 1.   -ay 2/95
 */
typedef int16 AttrNumber;

//#include "access/stratnum.h"

/*
 * Strategy numbers identify the semantics that particular operators have
 * with respect to particular operator classes.  In some cases a strategy
 * subtype (an OID) is used as further information.
 */
typedef uint16 StrategyNumber;


//typedef struct PScanKey{
//	int			sk_flags;		/* flags, see below */
//	AttrNumber	sk_attno;		/* table or index column number */
//	StrategyNumber sk_strategy; /* operator strategy number */
//	Oid			sk_subtype;		/* strategy subtype */
//	Oid			sk_collation;	/* collation to use, if needed */
//	Datum		sk_argument;	/* data to compare */
//} ScanKeyData;
//typedef ScanKeyData *ScanKey;


//extern declarations

extern ORAMState initORAMState(const char *name, int nBlocks, AMOFile* (*ofile)(), bool isHeap);

extern void FormIndexDatum_s(HeapTuple tuple, Datum *values, bool *isnull);


#endif 	/* SOE_H */