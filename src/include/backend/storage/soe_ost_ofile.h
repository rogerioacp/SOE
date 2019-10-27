#ifndef SOE_OST_OFILE_H
#define SOE_OST_OFILE_H

#include "soe_c.h"

#include "storage/soe_bufpage.h"
#include "storage/soe_bufmgr.h"
#include "storage/soe_ost_bufmgr.h"

#include <oram/ofile.h>

void ost_pageInit(Page page, int blkno, Size blocksize);

void  ost_fileRead(PLBlock block, const char *filename, const BlockNumber ob_blkno);
void 
ost_fileWrite(const PLBlock block, const char *filename, const BlockNumber ob_blkno);
extern AMOFile *ost_ofileCreate();
extern void ost_status(OSTreeState state);

extern void setclevelo(unsigned int nlevel);

#endif							/* SOE_OST_OFILE_H */
