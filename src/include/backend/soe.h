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

#include "access/htup.h"
#include "access/attnum.h"
#include "access/stratnum.h"
#include "postgres.h"


typedef struct PScanKey{
	int			sk_flags;		/* flags, see below */
	AttrNumber	sk_attno;		/* table or index column number */
	StrategyNumber sk_strategy; /* operator strategy number */
	Oid			sk_subtype;		/* strategy subtype */
	Oid			sk_collation;	/* collation to use, if needed */
	Datum		sk_argument;	/* data to compare */
} PScanKey;


void init(static char* tableName, static char* indexName, int tableNBlocks, int indexNBlocks);

void insert(HeapTupleData heapTuple);

HeapTupleData getTuple(PScanKey scankey);



#endif 	/* SOE_H */