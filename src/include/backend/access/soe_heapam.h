/*-------------------------------------------------------------------------
 *
 * soe_heapam.h
 * Bare bones copy of POSTGRES heap access method definitions to insert tuples
 * in an enclave.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/heapam.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SOE_HEAPAM_H
#define SOE_HEAPAM_H

#include "access/soe_htup.h"

#include "storage/soe_block.h"
#include "storage/soe_bufmgr.h"
#include "storage/soe_item.h"
#include "storage/soe_itemptr.h"

extern void heap_insert_s(VRelation relation,  Item tup, Size len, HeapTuple tuple);

#endif							/* SOE_HEAPAM_H */
