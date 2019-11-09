#ifndef SOE_HASH_OFILE_H
#define SOE_HASH_OFILE_H

#include "soe_c.h"

#include "storage/soe_bufpage.h"
#include <oram/ofile.h>

void		hash_pageInit(Page page, int blkno, Size blocksize);
extern AMOFile * hash_ofileCreate();

#endif							/* SOE_HASH_OFILE_H */
