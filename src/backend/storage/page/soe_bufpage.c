#include "access/soe_htup_details.h"
#include "storage/soe_bufpage.h"
#include "logger/logger.h"
#include "utils/soe_qsort.h"

#include <stdlib.h>

/*
 * PageGetHeapFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		reduced by the space needed for a new line pointer.
 *
 * The difference between this and PageGetFreeSpace is that this will return
 * zero if there are already MaxHeapTuplesPerPage line pointers in the page
 * and none are free.  We use this to enforce that no more than
 * MaxHeapTuplesPerPage line pointers are created on a heap page.  (Although
 * no more tuples than that could fit anyway, in the presence of redirected
 * or dead line pointers it'd be possible to have too many line pointers.
 * To avoid breaking code that assumes MaxHeapTuplesPerPage is a hard limit
 * on the number of line pointers, we make this extra check.)
 */
Size
PageGetHeapFreeSpace_s(Page page)
{
	Size		space;

	space = PageGetFreeSpace_s(page);
	if (space > 0)
	{
		OffsetNumber offnum,
					nline;

		/*
		 * Are there already MaxHeapTuplesPerPage line pointers in the page?
		 */
		nline = PageGetMaxOffsetNumber_s(page);
		if (nline >= MaxHeapTuplesPerPage)
		{
			if (PageHasFreeLinePointers_s((PageHeader) page))
			{
				/*
				 * Since this is just a hint, we must confirm that there is
				 * indeed a free line pointer
				 */
				for (offnum = FirstOffsetNumber; offnum <= nline; offnum = OffsetNumberNext_s(offnum))
				{
					ItemId		lp = PageGetItemId_s(page, offnum);

					if (!ItemIdIsUsed_s(lp))
						break;
				}

				if (offnum > nline)
				{
					/*
					 * The hint is wrong, but we can't clear it here since we
					 * don't have the ability to mark the page dirty.
					 */
					space = 0;
				}
			}
			else
			{
				/*
				 * Although the hint might be wrong, PageAddItem will believe
				 * it anyway, so we must believe it too.
				 */
				space = 0;
			}
		}
	}
	return space;
}


/*
 * Create a malloc'd copy of an index tuple.
 * function copy of CopyIndexTuple in indextuple.c
 */
IndexTuple
CopyIndexTuple_s(IndexTuple source)
{
	IndexTuple	result;
	Size		size;

	size = IndexTupleSize_s(source);
	result = (IndexTuple) malloc(size);
	memcpy(result, source, size);
	return result;
}

/*
 * PageGetFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		reduced by the space needed for a new line pointer.
 *
 * Note: this should usually only be used on index pages.  Use
 * PageGetHeapFreeSpace on heap pages.
 */
Size
PageGetFreeSpace_s(Page page)
{
	int			space;

	/*
	 * Use signed arithmetic here so that we behave sensibly if pd_lower >
	 * pd_upper.
	 */
	space = (int) ((PageHeader) page)->pd_upper -
		(int) ((PageHeader) page)->pd_lower;

	if (space < (int) sizeof(ItemIdData))
		return 0;
	space -= sizeof(ItemIdData);

	return (Size) space;
}


/*
 * Copy of same function from bufpage.c
 * PageGetFreeSpaceForMultipleTuples
 *		Returns the size of the free (allocatable) space on a page,
 *		reduced by the space needed for multiple new line pointers.
 *
 * Note: this should usually only be used on index pages.  Use
 * PageGetHeapFreeSpace on heap pages.
 */
Size
PageGetFreeSpaceForMultipleTuples_s(Page page, int ntups)
{
	int			space;

	/*
	 * Use signed arithmetic here so that we behave sensibly if pd_lower >
	 * pd_upper.
	 */
	space = (int) ((PageHeader) page)->pd_upper -
		(int) ((PageHeader) page)->pd_lower;

	if (space < (int) (ntups * sizeof(ItemIdData)))
		return 0;
	space -= ntups * sizeof(ItemIdData);

	return (Size) space;
}

void
PageInit_s(Page page, Size pageSize, Size specialSize)
{
	PageHeader	p = (PageHeader) page;

	specialSize = MAXALIGN_s(specialSize);

	/* Make sure all fields of page are zero, as well as unused space */
	MemSet_s(p, 0, pageSize);

	p->pd_flags = 0;
	p->pd_lower = SizeOfPageHeaderData;
	p->pd_upper = pageSize - specialSize;
	p->pd_special = pageSize - specialSize;
	PageSetPageSizeAndVersion_s(page, pageSize, PG_PAGE_LAYOUT_VERSION);
}



/*
 *	Copy of PageAddItemExtended in bufpage.c
 *
 *	Add an item to a page.  Return value is the offset at which it was
 *	inserted, or InvalidOffsetNumber if the item is not inserted for any
 *	reason.  A WARNING is issued indicating the reason for the refusal.
 *
 *	offsetNumber must be either InvalidOffsetNumber to specify finding a
 *	free item pointer, or a value between FirstOffsetNumber and one past
 *	the last existing item, to specify using that particular item pointer.
 *
 *	If offsetNumber is valid and flag PAI_OVERWRITE is set, we just store
 *	the item at the specified offsetNumber, which must be either a
 *	currently-unused item pointer, or one past the last existing item.
 *
 *	If offsetNumber is valid and flag PAI_OVERWRITE is not set, insert
 *	the item at the specified offsetNumber, moving existing items later
 *	in the array to make room.
 *
 *	If offsetNumber is not valid, then assign a slot by finding the first
 *	one that is both unused and deallocated.
 *
 *	If flag PAI_IS_HEAP is set, we enforce that there can't be more than
 *	MaxHeapTuplesPerPage line pointers on the page.
 *
 *	!!! EREPORT(ERROR) IS DISALLOWED HERE !!!
 */
OffsetNumber
PageAddItemExtended_s(Page page,
					Item item,
					Size size,
					OffsetNumber offsetNumber,
					int flags)
{
	PageHeader	phdr = (PageHeader) page;
	Size		alignedSize;
	int			lower;
	int			upper;
	ItemId		itemId;
	OffsetNumber limit;
	bool		needshuffle = false;

	/*
	 * Be wary about corrupted page pointers
	 */
	if (phdr->pd_lower < SizeOfPageHeaderData ||
		phdr->pd_lower > phdr->pd_upper ||
		phdr->pd_upper > phdr->pd_special ||
		phdr->pd_special > BLCKSZ)
		selog(ERROR, "corrupted page pointers: lower = %u, upper = %u, special = %u",phdr->pd_lower, phdr->pd_upper, phdr->pd_special);

	/*
	 * Select offsetNumber to place the new item at
	 */
	limit = OffsetNumberNext_s(PageGetMaxOffsetNumber_s(page));

	/* was offsetNumber passed in? */
	if (OffsetNumberIsValid_s(offsetNumber))
	{
		/* yes, check it */
		if ((flags & PAI_OVERWRITE) != 0)
		{
			if (offsetNumber < limit)
			{
				itemId = PageGetItemId_s(phdr, offsetNumber);
				if (ItemIdIsUsed_s(itemId) || ItemIdHasStorage_s(itemId))
				{
					selog(WARNING,"will not overwrite a used ItemId");
					return InvalidOffsetNumber;
				}
			}
		}
		else
		{
			if (offsetNumber < limit)
				needshuffle = true; /* need to move existing linp's */
		}
	}
	else
	{
		/* offsetNumber was not passed in, so find a free slot */
		/* if no free slot, we'll put it at limit (1st open slot) */
		if (PageHasFreeLinePointers_s(phdr))
		{
			/*
			 * Look for "recyclable" (unused) ItemId.  We check for no storage
			 * as well, just to be paranoid --- unused items should never have
			 * storage.
			 */
			for (offsetNumber = 1; offsetNumber < limit; offsetNumber++)
			{
				itemId = PageGetItemId_s(phdr, offsetNumber);
				if (!ItemIdIsUsed_s(itemId) && !ItemIdHasStorage_s(itemId))
					break;
			}
			if (offsetNumber >= limit)
			{
				/* the hint is wrong, so reset it */
				PageClearHasFreeLinePointers_s(phdr);
			}
		}
		else
		{
			/* don't bother searching if hint says there's no free slot */
			offsetNumber = limit;
		}
	}

	/* Reject placing items beyond the first unused line pointer */
	if (offsetNumber > limit)
	{
		//LOG error
		selog(WARNING, "specified item offset is too large");
		return InvalidOffsetNumber;
	}

	/* Reject placing items beyond heap boundary, if heap */
	if ((flags & PAI_IS_HEAP) != 0 && offsetNumber > MaxHeapTuplesPerPage)
	{
		//Log error
		selog(WARNING, "can't put more than MaxHeapTuplesPerPage items in a heap page");
		return InvalidOffsetNumber;
	}

	/*
	 * Compute new lower and upper pointers for page, see if it'll fit.
	 *
	 * Note: do arithmetic as signed ints, to avoid mistakes if, say,
	 * alignedSize > pd_upper.
	 */
	if (offsetNumber == limit || needshuffle)
		lower = phdr->pd_lower + sizeof(ItemIdData);
	else
		lower = phdr->pd_lower;

	alignedSize = MAXALIGN_s(size);

	upper = (int) phdr->pd_upper - (int) alignedSize;

	if (lower > upper)
		return InvalidOffsetNumber;

	/*
	 * OK to insert the item.  First, shuffle the existing pointers if needed.
	 */
	itemId = PageGetItemId_s(phdr, offsetNumber);

	if (needshuffle)
		memmove(itemId + 1, itemId,
				(limit - offsetNumber) * sizeof(ItemIdData));

	/* set the item pointer */
	ItemIdSetNormal_s(itemId, upper, size);

	/* copy the item's data onto the page */
	memcpy((char *) page + upper, item, size);

	/* adjust page header */
	phdr->pd_lower = (LocationIndex) lower;
	phdr->pd_upper = (LocationIndex) upper;

	return offsetNumber;
}



/*
 * sorting support for PageRepairFragmentation and PageIndexMultiDelete
 */
typedef struct itemIdSortData
{
	uint16		offsetindex;	/* linp array index */
	int16		itemoff;		/* page offset of item data */
	uint16		alignedlen;		/* MAXALIGN(item data len) */
} itemIdSortData;
typedef itemIdSortData *itemIdSort;

static int
itemoffcompare_s(const void *itemidp1, const void *itemidp2)
{
	/* Sort in decreasing itemoff order */
	return ((itemIdSort) itemidp2)->itemoff -
		((itemIdSort) itemidp1)->itemoff;
}

/*
 * After removing or marking some line pointers unused, move the tuples to
 * remove the gaps caused by the removed items.
 */
static void
compactify_tuples_s(itemIdSort itemidbase, int nitems, Page page)
{
	PageHeader	phdr = (PageHeader) page;
	Offset		upper;
	int			i;
	//selog(DEBUG1, "sort items to compact");
	/* sort itemIdSortData array into decreasing itemoff order */
	qsort_s((char *) itemidbase, nitems, sizeof(itemIdSortData),
		  itemoffcompare_s);

	upper = phdr->pd_special;
	for (i = 0; i < nitems; i++)
	{
		itemIdSort	itemidptr = &itemidbase[i];
		ItemId		lp;

		lp = PageGetItemId_s(page, itemidptr->offsetindex + 1);
		upper -= itemidptr->alignedlen;
		memmove((char *) page + upper,
				(char *) page + itemidptr->itemoff,
				itemidptr->alignedlen);
		lp->lp_off = upper;
	}
	//selog(DEBUG1, "Page has %d items and New upper position is %d", nitems, upper);
	phdr->pd_upper = upper;
}




/*
 * PageIndexMultiDelete
 *
 * This routine handles the case of deleting multiple tuples from an
 * index page at once.  It is considerably faster than a loop around
 * PageIndexTupleDelete ... however, the caller *must* supply the array
 * of item numbers to be deleted in item number order!
 */
void
PageIndexMultiDelete_s(Page page, OffsetNumber *itemnos, int nitems)
{
	PageHeader	phdr = (PageHeader) page;
	Offset		pd_lower = phdr->pd_lower;
	Offset		pd_upper = phdr->pd_upper;
	Offset		pd_special = phdr->pd_special;
	itemIdSortData itemidbase[MaxIndexTuplesPerPage];
	ItemIdData	newitemids[MaxIndexTuplesPerPage];
	itemIdSort	itemidptr;
	ItemId		lp;
	int			nline,
				nused;
	Size		totallen;
	Size		size;
	unsigned	offset;
	int			nextitm;
	OffsetNumber offnum;


	/*
	 * As with PageRepairFragmentation, paranoia seems justified.
	 */
	if (pd_lower < SizeOfPageHeaderData ||
		pd_lower > pd_upper ||
		pd_upper > pd_special ||
		pd_special > BLCKSZ ||
		pd_special != MAXALIGN_s(pd_special)){
		selog(ERROR, "1-corrupted page pointers: lower = %u, upper = %u, special = %u", pd_lower, pd_upper, pd_special);
	}
	

	/*
	 * Scan the item pointer array and build a list of just the ones we are
	 * going to keep.  Notice we do not modify the page yet, since we are
	 * still validity-checking.
	 */
	nline = PageGetMaxOffsetNumber_s(page);
	itemidptr = itemidbase;
	totallen = 0;
	nused = 0;
	nextitm = 0;
	for (offnum = FirstOffsetNumber; offnum <= nline; offnum = OffsetNumberNext_s(offnum))
	{
		lp = PageGetItemId_s(page, offnum);
		size = ItemIdGetLength_s(lp);
		offset = ItemIdGetOffset_s(lp);
		
		if (offset < pd_upper ||
			(offset + size) > pd_special ||
			offset != MAXALIGN_s(offset)){
			selog(ERROR, "2-corrupted item pointer: offset = %u, length = %u",offset, (unsigned int) size);
		}

		if (nextitm < nitems && offnum == itemnos[nextitm])
		{
			/* skip item to be deleted */
			nextitm++;
		}
		else
		{
			itemidptr->offsetindex = nused; /* where it will go */
			itemidptr->itemoff = offset;
			itemidptr->alignedlen = MAXALIGN_s(size);
			totallen += itemidptr->alignedlen;
			newitemids[nused] = *lp;
			itemidptr++;
			nused++;
		}
	}

	/* this will catch invalid or out-of-order itemnos[] */
	if (nextitm != nitems)
		selog(ERROR, "incorrect index offsets supplied");

	if (totallen > (Size) (pd_special - pd_lower))
		selog(ERROR, "corrupted item lengths: total %u, available space %u",
						(unsigned int) totallen, pd_special - pd_lower);

	/*
	 * Looks good. Overwrite the line pointers with the copy, from which we've
	 * removed all the unused items.
	 */
	memcpy(phdr->pd_linp, newitemids, nused * sizeof(ItemIdData));
	phdr->pd_lower = SizeOfPageHeaderData + nused * sizeof(ItemIdData);
	//selog(DEBUG1, "Going to compact tuples of old page");
	/* and compactify the tuple data */
	compactify_tuples_s(itemidbase, nused, page);
}



/*
 * PageGetExactFreeSpace
 *		Returns the size of the free (allocatable) space on a page,
 *		without any consideration for adding/removing line pointers.
 */
Size
PageGetExactFreeSpace_s(Page page)
{
	int			space;

	/*
	 * Use signed arithmetic here so that we behave sensibly if pd_lower >
	 * pd_upper.
	 */
	space = (int) ((PageHeader) page)->pd_upper -
		(int) ((PageHeader) page)->pd_lower;

	if (space < 0)
		return 0;

	return (Size) space;
}

/*
 * PageRestoreTempPage
 *		Copy temporary page back to permanent page after special processing
 *		and release the temporary page.
 */
void
PageRestoreTempPage_s(Page tempPage, Page oldPage)
{
	Size		pageSize;

	pageSize = PageGetPageSize_s(tempPage);
	memcpy((char *) oldPage, (char *) tempPage, pageSize);

	free(tempPage);
}


/*
 * PageGetTempPage
 *		Get a temporary page in local memory for special processing.
 *		The returned page is not initialized at all; caller must do that.
 */
Page
PageGetTempPage_s(Page page)
{
	Size		pageSize;
	Page		temp;

	pageSize = PageGetPageSize_s(page);
	temp = (Page) malloc(pageSize);

	return temp;
}
