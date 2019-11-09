#ifndef SOE_NBTREE_OFILE_H
#define SOE_NBTREE_OFILE_H

#include "soe_c.h"

#include "storage/soe_bufpage.h"
#include <oram/ofile.h>

void		nbtree_pageInit(Page page, int blkno, Size blocksize);
extern AMOFile * nbtree_ofileCreate();

#endif							/* SOE_NBTREE_OFILE_H */
