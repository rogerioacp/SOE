/*-------------------------------------------------------------------------
 *
 * heapam.h
 *	  The public API for the SOE heapam methods that replace the postgres 
 *    heapam methods.
 *
 * Copyright (c) 2018-2019, HASLab
 *
 * src/include/heap/heapam.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_HEAPAM_H
#define SOE_HEAPAN_H

#include "storage/item.h"
#include "access/htup_details.h"

void heap_insert(Item item, Size size);



#endif 	/* SOE_HEAPAM_H */