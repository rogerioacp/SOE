
#include "heap/heapam.c"

void heap_insert(Item item, Size size){


    /**
     * When an insert operation is received on the enclave, the enclave has to index the inserted value and
     * store the tuple on the table relation.
     *
     * For now we are not indexing the value and are just inserting it on a block in the relationship file that has
     * free space.
     **/
    char* page;
    int result;
    OblivPageOpaque oopaque;


   // elog(DEBUG1, "Reading block %d from oram file", currentBlock);
    result = read(&page, (BlockNumber) currentBlock, stateTable);
   // elog(DEBUG1, "Read result is %d", result);
    if(result == DUMMY_BLOCK){

        /**
         * When the read returns a DUMMY_BLOCK page  it means its the first time the page is read from the disk.
         * As such, its going to be the first time a tuple is going to be written to the page and the special space of
         * the page has to be tagged with the real block block number so future accesses know that it's no longer a
         * dummy block.
         * As such, a new page needs to be allocated and initialized so the tuple can be added.
         **/
        elog(DEBUG1, "First time PAGE is Read. Going to initialize a new one.");
        page = (char*) malloc(BLCKSZ);
        //When this code is in the Enclave, PageInit will have be an internal function or an OCALL.
        PageInit(page, BLCKSZ, sizeof(OblivPageOpaqueData));
        oopaque = (OblivPageOpaque) PageGetSpecialPointer(page);
        oopaque->o_blkno = currentBlock;
       // elog(DEBUG1, "Page allocated and initalized.");
    }


   // elog(DEBUG1, "Page from block %d read", currentBlock);

    OffsetNumber limit;
    OffsetNumber offsetNumber;
    ItemId itemId;
    int	lower;
    int upper;
    Size alignedSize;

    PageHeader phdr = (PageHeader) page;
    int flags = true; //Defined on the invocation of PageAddItem in RelationPutHeapTuple

    /*
     * Be wary about corrupted page pointers
     */
    if (phdr->pd_lower < SizeOfPageHeaderData ||
        phdr->pd_lower > phdr->pd_upper ||
        phdr->pd_upper > phdr->pd_special ||
        phdr->pd_special > BLCKSZ)
        ereport(PANIC,
                (errcode(ERRCODE_DATA_CORRUPTED),
                        errmsg("corrupted page pointers: lower = %u, upper = %u, special = %u",
                               phdr->pd_lower, phdr->pd_upper, phdr->pd_special)));

    /*
     * Select offsetNumber to place the new item at
     */
    limit = OffsetNumberNext(PageGetMaxOffsetNumber(page));

    /**
     * Don't bother searching, heap tuples are never updated or deleted.
     * The prototype is only doing sequential insertions*/

    offsetNumber = limit;


    /* Reject placing items beyond heap boundary, if heap */
    if ((flags & PAI_IS_HEAP) != 0 && offsetNumber > MaxHeapTuplesPerPage)
    {
        elog(WARNING, "can't put more than MaxHeapTuplesPerPage items in a heap page");
        exit(1);
    }
    /*
     * Compute new lower and upper pointers for page, see if it'll fit.
     *
     * Note: do arithmetic as signed ints, to avoid mistakes if, say,
     * alignedSize > pd_upper.
     */

    lower = phdr->pd_lower + sizeof(ItemIdData);

    alignedSize = MAXALIGN(size);

    upper = (int) phdr->pd_upper - (int) alignedSize;

    /*
     * OK to insert the item.  First, shuffle the existing pointers if needed.
     */
    itemId = PageGetItemId(phdr, offsetNumber);

    /* set the item pointer */
    ItemIdSetNormal(itemId, upper, size);
    //elog(DEBUG1, "Writting item to offset %d\n", upper);

    /* copy the item's data onto the page */
    memcpy((char *) page + upper, item, size);

    /* adjust page header */
    phdr->pd_lower = (LocationIndex) lower;
    phdr->pd_upper = (LocationIndex) upper;

    //elog(DEBUG1, "page header lower %d\n", lower);
    //elog(DEBUG1, "page header lower %d\n", upper);

    elog(DEBUG1, " --------------------  Going to update page on disk");
    result  = write(page, BLCKSZ, (BlockNumber) currentBlock, stateTable);
    /**Whether the page was allocated in this function or in page_read on the obliv_ofile.c, it can be freed as it will
     *no longer be used. Test for memory leaks.
     */
    free(page);
}