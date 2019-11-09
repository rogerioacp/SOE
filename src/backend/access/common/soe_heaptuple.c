
#include "soe_c.h"
#include "access/soe_htup_details.h"
#include "access/soe_tupdesc.h"
#include "access/soe_tupmacs.h"

/*
 * heap_compute_data_size
 *		Determine size of the data area of a tuple to be constructed
 */
Size
heap_compute_data_size_s(TupleDesc tupleDesc,
						 Datum * values,
						 bool *isnull)
{
	Size		data_length = 0;
	int			i;
	int			numberOfAttributes = tupleDesc->natts;

	for (i = 0; i < numberOfAttributes; i++)
	{
		Datum		val;
		Form_pg_attribute atti;

		if (isnull[i])
			continue;

		val = values[i];
		atti = tupleDesc->attrs;
		/* TupleDescAttr_s(tupleDesc, i); */

		/**
		* The original postgres function had more code to handle data types
		* with variable length and to try to compress and pack the values.
		* However, since the current prototype uses this function
		* for hash indexes the function was simplified because the hash keys
		* are fixed size and can not be compressed.
		*
		*/
		data_length = att_align_datum_s(data_length, atti->attalign,
										atti->attlen, val);
		/**
		* The original hash function to calculate the data size handles
		* variable length datums and requires integrating more code
		* to deal with those cases. To simplify for the prototype.
		* we assume this is only used for hash indexes which the datums
		* is a single hash key with a fixed size.
		* Its actually an int
		*/

		/*
		 * data_length = att_addlength_datum_s(data_length, atti->attlen,
		 * val);
		 */
		/* data_length = data_length + (strlen((char*) val)+1); */

		/* README atti->attlen has an invalid value */
		if (tupleDesc->isnbtree)
		{
			data_length = data_length + (strlen((char *) val) + 1);
		}
		else
		{
			data_length = data_length + atti->attlen;
		}
	}

	return data_length;
}



/*
 * Per-attribute helper for heap_fill_tuple and other routines building tuples.
 *
 * Fill in either a data value or a bit in the null bitmask
 */
static inline void
fill_val_s(Form_pg_attribute att,
		   bits8 * *bit,
		   int *bitmask,
		   char **dataP,
		   uint16 * infomask,
		   Datum datum,
		   bool isnull,
		   Size data_size)
{
	Size		data_length;
	char	   *data = *dataP;

	/*
	 * If we're building a null bitmap, set the appropriate bit for the
	 * current column value here.
	 */
	if (bit != NULL)
	{
		if (*bitmask != HIGHBIT)
			*bitmask <<= 1;
		else
		{
			*bit += 1;
			**bit = 0x0;
			*bitmask = 1;
		}

		if (isnull)
		{
			*infomask |= HEAP_HASNULL;
			return;
		}

		**bit |= *bitmask;
	}

	/*
	 * XXX we use the att_align macros on the pointer value itself, not on an
	 * offset.  This is a bit of a hack.
	 */
	if (att->attbyval)
	{
		/* pass-by-value */
		data = (char *) att_align_nominal_s(data, att->attalign);
		store_att_byval_s(data, datum, att->attlen);
		data_length = att->attlen;
	}
	else if (att->attlen == -1)
	{

		/***
		 *
		 * The original postgres had more conditions. This is just working
		 * for btees on strings with small size.
		 *
		 */
		Pointer		val = DatumGetPointer_s(datum);

		data_length = data_size;
		/* selog(DEBUG1, "Insert char datum with size %d", data_length); */

		memcpy(data, val, data_length);
	}
	else
	{
		data_length = 0;

		selog(ERROR, "ERROR - Unexpected condition on fill_val_s - 2");
		/***
		 * The Original postgres code had more conditions which were discarded
		 * because after debuging the code only the first condition was used
		 * for hash indexed. If other conditions are necessary they can be
		 * latter included.
		 */
	}

	data += data_length;
	*dataP = data;
}


/*
 * heap_fill_tuple
 *		Load data portion of a tuple from values/isnull arrays
 *
 * We also fill the null bitmap (if any) and set the infomask bits
 * that reflect the tuple's data contents.
 *
 * NOTE: it is now REQUIRED that the caller have pre-zeroed the data area.
 */
void
heap_fill_tuple_s(TupleDesc tupleDesc,
				  Datum * values, bool *isnull,
				  char *data, Size data_size,
				  uint16 * infomask, bits8 * bit)
{
	bits8	   *bitP;
	int			bitmask;
	int			i;
	int			numberOfAttributes = tupleDesc->natts;

	if (bit != NULL)
	{
		bitP = &bit[-1];
		bitmask = HIGHBIT;
	}
	else
	{
		/* just to keep compiler quiet */
		bitP = NULL;
		bitmask = 0;
	}

	*infomask &= ~(HEAP_HASNULL | HEAP_HASVARWIDTH | HEAP_HASEXTERNAL);

	for (i = 0; i < numberOfAttributes; i++)
	{
		Form_pg_attribute attr = TupleDescAttr_s(tupleDesc, i);

		fill_val_s(attr,
				   bitP ? &bitP : NULL,
				   &bitmask,
				   &data,
				   infomask,
				   values ? values[i] : PointerGetDatum_s(NULL),
				   isnull ? isnull[i] : true, data_size);
	}
	/* if((data-start) != data_size){ */
	/* selog(DEBUG1, "datum was not copied correctly") */
	/* } */
}
