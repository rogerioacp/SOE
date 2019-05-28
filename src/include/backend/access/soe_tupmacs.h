/*-------------------------------------------------------------------------
 *
 * tupmacs.h
 * BareBones copy of Tuple macros used by both index tuples and heap tuples
 * for enclave execution.
 *
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/tupmacs.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SOE_TUPMACS_H
#define SOE_TUPMACS_H

#include <string.h>
#include "logger/logger.h"



/*
 * att_align_nominal aligns the given offset as needed for a datum of alignment
 * requirement attalign, ignoring any consideration of packed varlena datums.
 * There are three main use cases for using this macro directly:
 *	* we know that the att in question is not varlena (attlen != -1);
 *	  in this case it is cheaper than the above macros and just as good.
 *	* we need to estimate alignment padding cost abstractly, ie without
 *	  reference to a real tuple.  We must assume the worst case that
 *	  all varlenas are aligned.
 *	* within arrays, we unconditionally align varlenas (XXX this should be
 *	  revisited, probably).
 *
 * The attalign cases are tested in what is hopefully something like their
 * frequency of occurrence.
 */
#define att_align_nominal_s(cur_offset, attalign) \
( \
	((attalign) == 'i') ? INTALIGN_S(cur_offset) : \
	 (((attalign) == 'c') ? (uintptr_t) (cur_offset) : \
	  (((attalign) == 'd') ? DOUBLEALIGN_S(cur_offset) : \
	   ( \
			SHORTALIGN_S(cur_offset) \
	   ))) \
)

/*
 * att_align_datum aligns the given offset as needed for a datum of alignment
 * requirement attalign and typlen attlen.  attdatum is the Datum variable
 * we intend to pack into a tuple (it's only accessed if we are dealing with
 * a varlena type).  Note that this assumes the Datum will be stored as-is;
 * callers that are intending to convert non-short varlena datums to short
 * format have to account for that themselves.
 */
#define att_align_datum_s(cur_offset, attalign, attlen, attdatum) \
( \
	((attlen) == -1 && VARATT_IS_SHORT_S(DatumGetPointer_s(attdatum))) ? \
	(uintptr_t) (cur_offset) : \
	att_align_nominal_s(cur_offset, attalign) \
)


/*
 * att_addlength_datum increments the given offset by the space needed for
 * the given Datum variable.  attdatum is only accessed if we are dealing
 * with a variable-length attribute.
 */
/*#define att_addlength_datum_s(cur_offset, attlen, attdatum) \
	att_addlength_pointer_s(cur_offset, attlen, DatumGetPointer_s(attdatum))*/

/*
 * att_addlength_pointer performs the same calculation as att_addlength_datum,
 * but is used when walking a tuple --- attptr is the pointer to the field
 * within the tuple.
 *
 * Note: some callers pass a "char *" pointer for cur_offset.  This is
 * actually perfectly OK, but probably should be cleaned up along with
 * the same practice for att_align_pointer.
 */
/*#define att_addlength_pointer_s(cur_offset, attlen, attptr) \
( \
	((attlen) > 0) ? \
	( \
		(cur_offset) + (attlen) \
	) \
	: (((attlen) == -1) ? \
	( \
		(cur_offset) + VARSIZE_ANY_S(attptr) \
	) \
	: \
	( \
		(cur_offset) + (strlen((char *) (attptr)) + 1) \
	)) \
)*/

/*
 * store_att_byval is a partial inverse of fetch_att: store a given Datum
 * value into a tuple data area at the specified address.  However, it only
 * handles the byval case, because in typical usage the caller needs to
 * distinguish by-val and by-ref cases anyway, and so a do-it-all macro
 * wouldn't be convenient.
 */
#if SIZEOF_DATUM == 8

#define store_att_byval_s(T,newdatum,attlen) \
	do { \
		switch (attlen) \
		{ \
			case sizeof(char): \
				*(char *) (T) = DatumGetChar_s(newdatum); \
				break; \
			case sizeof(int16): \
				*(int16 *) (T) = DatumGetInt16_s(newdatum); \
				break; \
			case sizeof(int32): \
				*(int32 *) (T) = DatumGetInt32_s(newdatum); \
				break; \
			case sizeof(Datum): \
				*(Datum *) (T) = (newdatum); \
				break; \
			default: \
				selog(ERROR, "unsupported byval length: %d", \
					 (int) (attlen)); \
				break; \
		} \
	} while (0)
#else							/* SIZEOF_DATUM != 8 */

#define store_att_byval_s(T,newdatum,attlen) \
	do { \
		switch (attlen) \
		{ \
			case sizeof(char): \
				*(char *) (T) = DatumGetChar_s(newdatum); \
				break; \
			case sizeof(int16): \
				*(int16 *) (T) = DatumGetInt16_s(newdatum); \
				break; \
			case sizeof(int32): \
				*(int32 *) (T) = DatumGetInt32_s(newdatum); \
				break; \
			default: \
				selog(ERROR, "unsupported byval length: %d", \
					 (int) (attlen)); \
				break; \
		} \
	} while (0)
#endif							/* SIZEOF_DATUM == 8 */

#endif
