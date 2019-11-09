/*-------------------------------------------------------------------------
 *
 * soe genam.h
 *	bare bones copy of POSTGRES generalized index access method definitions.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/genam.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SOE_GENAM_H
#define SOE_GENAM_H

#include "access/soe_itup.h"
#include "storage/soe_bufmgr.h"


/*
 * index access method support routines (in genam.c)
 */
extern IndexScanDesc RelationGetIndexScan(VRelation indexRelation,
										  int nkeys, int norderbys);


#endif							/* GENAM_H */
