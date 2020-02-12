#ifndef SOE_OST_OFILE_H
#define SOE_OST_OFILE_H

#include "soe_c.h"

#include "storage/soe_bufpage.h"
#include "storage/soe_bufmgr.h"
#include "storage/soe_ost_bufmgr.h"

#include <oram/ofile.h>


extern void init_root(const char* filename);
extern void ost_status(OSTreeState state);
extern AMOFile * ost_ofileCreate();

void		ost_pageInit(Page page, int blkno, Size blocksize);

void		ost_fileRead(FileHandler handler, PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData);
void        ost_fileWrite(FileHandler handler, const PLBlock block, const char *filename, const BlockNumber ob_blkno, void *appData);


/* extern void setclevelo(unsigned int nlevel); */

#endif							/* SOE_OST_OFILE_H */
