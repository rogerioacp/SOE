/*-------------------------------------------------------------------------
 *
 * genam.c
 * bare bones copy of general index access method routines for enclave
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/index/genam.c
 *
 * NOTES
 *	  many of the old access method routines have been turned into
 *	  macros and moved to genam.h -cim 4/30/91
 *
 *-------------------------------------------------------------------------
 */



/* ----------------
 *	RelationGetIndexScan -- Create and fill an IndexScanDesc.
 *
 *		This routine creates an index scan structure and sets up initial
 *		contents for it.
 *
 *		Parameters:
 *				indexRelation -- index relation for scan.
 *				nkeys -- count of scan keys (index qual conditions).
 *				norderbys -- count of index order-by operators.
 *
 *		Returns:
 *				An initialized IndexScanDesc.
 * ----------------
 */
IndexScanDesc
RelationGetIndexScan(VRelation indexRelation, int nkeys, int norderbys)
{
	IndexScanDesc scan;

	scan = (IndexScanDesc) palloc(sizeof(IndexScanDescData));

	scan->heapRelation = NULL;	/* may be set later */
	scan->indexRelation = indexRelation;
	scan->xs_snapshot = InvalidSnapshot;	/* caller must initialize this */
	scan->numberOfKeys = nkeys;
	scan->numberOfOrderBys = norderbys;

	/*
	 * We allocate key workspace here, but it won't get filled until amrescan.
	 */
	if (nkeys > 0)
		scan->keyData = (ScanKey) palloc(sizeof(ScanKeyData) * nkeys);
	else
		scan->keyData = NULL;
	if (norderbys > 0)
		scan->orderByData = (ScanKey) palloc(sizeof(ScanKeyData) * norderbys);
	else
		scan->orderByData = NULL;

	scan->xs_want_itup = false; /* may be set later */

	/*
	 * During recovery we ignore killed tuples and don't bother to kill them
	 * either. We do this because the xmin on the primary node could easily be
	 * later than the xmin on the standby node, so that what the primary
	 * thinks is killed is supposed to be visible on standby. So for correct
	 * MVCC for queries during recovery we must ignore these hints and check
	 * all tuples. Do *not* set ignore_killed_tuples to true when running in a
	 * transaction that was started during recovery. xactStartedInRecovery
	 * should not be altered by index AMs.
	 */
	scan->kill_prior_tuple = false;
	scan->xactStartedInRecovery = TransactionStartedDuringRecovery();
	scan->ignore_killed_tuples = !scan->xactStartedInRecovery;

	scan->opaque = NULL;

	scan->xs_itup = NULL;
	scan->xs_itupdesc = NULL;
	scan->xs_hitup = NULL;
	scan->xs_hitupdesc = NULL;

	ItemPointerSetInvalid(&scan->xs_ctup.t_self);
	scan->xs_ctup.t_data = NULL;
	scan->xs_cbuf = InvalidBuffer;
	scan->xs_continue_hot = false;

	return scan;
}