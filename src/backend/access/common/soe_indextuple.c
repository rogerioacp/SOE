/*-------------------------------------------------------------------------
 *
 * indextuple.c
 *	   This file contains index tuple accessor and mutator routines,
 *	   as well as various tuple utilities.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/common/indextuple.c
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_itup.h"
#include "access/soe_tupdesc.h"
#include "access/soe_htup_details.h"
#include "logger/logger.h"
#include <stdlib.h>


/* ----------------------------------------------------------------
 *				  index_ tuple interface routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *		index_form_tuple
 *
 *		This shouldn't leak any memory; otherwise, callers such as
 *		tuplesort_putindextuplevalues() will be very unhappy.
 *
 *		This shouldn't perform external table access provided caller
 *		does not pass values that are stored EXTERNAL.
 * ----------------
 */
IndexTuple
index_form_tuple_s(TupleDesc tupleDescriptor,
				   Datum * values,
				   bool *isnull)
{
	char	   *tp;				/* tuple pointer */
	IndexTuple	tuple;			/* return tuple */
	Size		size,
				data_size,
				hoff;
	int			i;
	unsigned short infomask = 0;
	bool		hasnull = false;
	uint16		tupmask = 0;
	int			numberOfAttributes = tupleDescriptor->natts;

	/**
	*
	* The original function for index_form_tuple had more code to handler
	* tuples of variable length and tried to compress them by toasting the
	* values. This code has been removed for now for simplicity.  The
	* prototype is going to assume fixed size data that can not be toasted.
	*/

	if (numberOfAttributes > INDEX_MAX_KEYS)
		selog(ERROR, "number of index columns (%d) exceeds limit (%d)", numberOfAttributes, INDEX_MAX_KEYS);



	for (i = 0; i < numberOfAttributes; i++)
	{
		if (isnull[i])
		{
			hasnull = true;
			break;
		}
	}

	if (hasnull)
		infomask |= INDEX_NULL_MASK;

	hoff = IndexInfoFindDataOffset_s(infomask);
	data_size = heap_compute_data_size_s(tupleDescriptor,
										 values, isnull);

	/*
	 * selog(DEBUG1, "info mask offset %d <-> heap compute data size is %d",
	 * hoff, data_size);
	 */
	size = hoff + data_size;
	size = MAXALIGN_s(size);	/* be conservative */
	/* selog(DEBUG1, "maxalign data size is %d", size); */
	tp = (char *) malloc(size);
	tuple = (IndexTuple) tp;

	heap_fill_tuple_s(tupleDescriptor,
					  values,
					  isnull,
					  (char *) tp + hoff,
					  data_size,
					  &tupmask,
					  (hasnull ? (bits8 *) tp + sizeof(IndexTupleData) : NULL));



	/*
	 * We do this because heap_fill_tuple wants to initialize a "tupmask"
	 * which is used for HeapTuples, but we want an indextuple infomask. The
	 * only relevant info is the "has variable attributes" field. We have
	 * already set the hasnull bit above.
	 */
	if (tupmask & HEAP_HASVARWIDTH)
		infomask |= INDEX_VAR_MASK;


	/*
	 * Here we make sure that the size will fit in the field reserved for it
	 * in t_info.
	 */
	if ((size & INDEX_SIZE_MASK) != size)
		selog(ERROR, "index row requires %zu bytes, maximum size is %zu",
			  size, (Size) INDEX_SIZE_MASK);

	infomask |= size;

	/*
	 * initialize metadata
	 */
	tuple->t_info = infomask;
	return tuple;
}
