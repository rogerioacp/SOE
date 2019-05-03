/*-------------------------------------------------------------------------
 *
 * Copy of postgres qsort
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_QSORT_H
#define SOE_QSORT__H

void pg_qsort(void *base, size_t nel, size_t elsize,
		 int (*cmp) (const void *, const void *));
#define qsort(a,b,c,d) pg_qsort(a,b,c,d)

#endif
