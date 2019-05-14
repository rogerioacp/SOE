#ifndef SOE_HEAP_OFILE_H
#define SOE_HEAP_OFILE_H

#include "soe_c.h"
#include "storage/soe_bufpage.h"
#include <oram/ofile.h>


//Data structure of the contents stored on every oblivious page.
typedef struct OblivPageOpaqueData
{
    int		o_blkno; //original block number. This should be encrypted.

} OblivPageOpaqueData;

typedef OblivPageOpaqueData *OblivPageOpaque;

void heap_pageInit(Page page, BlockNumber blkno, Size blocksize);

extern AMOFile *heap_ofileCreate();

#endif							/* SOE_HEAP_OFILE_H */
