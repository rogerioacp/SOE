/*-------------------------------------------------------------------------
 *
 * nbtutils.c
 *	  Utility code for Postgres btree implementation.
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtutils.c
 *
 *-------------------------------------------------------------------------
 */

#include "access/soe_ost.h"
#include "access/soe_itup.h"
#include "logger/logger.h"



/*
 * Test whether an indextuple satisfies all the scankey conditions.
 *
 * If so, return the address of the index tuple on the index page.
 * If not, return NULL.
 *
 * If the tuple fails to pass the qual, we also determine whether there's
 * any need to continue the scan beyond this tuple, and set *continuescan
 * accordingly.  See comments for _bt_preprocess_keys(), above, about how
 * this is done.
 *
 * scan: index scan descriptor (containing a search-type scankey)
 * page: buffer page containing index tuple
 * offnum: offset number of index tuple (must be a valid item!)
 * dir: direction we are scanning in
 * continuescan: output parameter (will be set correctly in all cases)
 *
 * Caller must hold pin and lock on the index page.
 */
IndexTuple
_bt_checkkeys_ost(IndexScanDesc scan,
				  Page page, OffsetNumber offnum,
				  bool *continuescan)
{
	ItemId		iid = PageGetItemId_s(page, offnum);
	IndexTuple	tuple;
	char	   *keyValue;
	char	   *datum;
	int			test;


	*continuescan = false;		/* default assumption */

	/*
	 * If the scan specifies not to return killed tuples, then we treat a
	 * killed tuple as not passing the qual.  Most of the time, it's a win to
	 * not bother examining the tuple's index keys, but just return
	 * immediately with continuescan = true to proceed to the next tuple.
	 * However, if this is the last tuple on the page, we should check the
	 * index keys to prevent uselessly advancing to the next page.
	 */


	tuple = (IndexTuple) PageGetItem_s(page, iid);
	datum = VARDATA_ANY_S(DatumGetBpCharPP_S(index_getattr_s(tuple)));
	/* selog(DEBUG1, "Current datum is %s", datum); */
	/* datumSize = strlen(datum)+1; */
	keyValue = scan->keyData->sk_argument;
	/* the prototype assumes string comparisons */
	test = (int32) strncmp(datum, keyValue, strlen(datum)-1);

    /**
	* Look at soe_nbtsearch.c function _bt_first_s to which operations the
	* opoids correspond to.
	*/
	if ((scan->opoid == 1058 && test < 0) || (scan->opoid == 1059 && test <= 0) || (scan->opoid == 1054 && test == 0) || (scan->opoid == 1061 && test >= 0) || (scan->opoid == 1060 && test > 0))
	{
		*continuescan = true;
		return tuple;
	}
	else
	{
		*continuescan = false;
		return NULL;

	}
}

/*
 * free a retracement stack made by _bt_search.
 */
void
_bt_freestack_ost(BTStackOST stack)
{
	BTStackOST	ostack;

	while (stack != NULL)
	{
		ostack = stack;
		stack = stack->bts_parent;
		free(ostack);
	}
}
