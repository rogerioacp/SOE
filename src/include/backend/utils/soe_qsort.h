/*-------------------------------------------------------------------------
 *
 * Copy of postgres qsort
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_QSORT_H
#define SOE_QSORT_H

void		pg_qsort_s(void *base, size_t nel, size_t elsize,
					   int (*cmp) (const void *, const void *));
#define qsort_s(a,b,c,d) pg_qsort_s(a,b,c,d)

#endif
