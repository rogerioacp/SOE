#ifndef SOE_OST_BUFMGR_H
#define SOE_OST_BUFMGR_H

#include "storage/soe_buf.h"
#include "storage/soe_bufpage.h"
#include "storage/soe_block.h"


#include <oram/oram.h>
#include <collectc/list.h>

/*
 * BufferGetPageSize
 *      Returns the page size within a buffer.
 *
 * Notes:
 *      Assumes buffer is valid.
 *
 *      The buffer can be a raw disk block and need not contain a valid
 *      (formatted) disk page.
 */
/* XXX should dig out of buffer descriptor */
#define BufferGetPageSize_ost(vbuffer, buffer) \
( \
    (Size)BLCKSZ \
)

#define BufferIsValid_ost(vbuffer, bufnum) \
( \
    (bufnum) != InvalidBuffer  \
)

typedef struct OSTreeState{
    int* fanouts;
    int nlevels;
    unsigned int iOid;
    ORAMState *orams;
    char* iname;
}*OSTreeState;


//Read only Relation to execute the OST protocol.
typedef struct OSTRelation{

	unsigned int rd_id; // Original Relation Oid
	OSTreeState osts;

	void *rd_amcache; //used to cache metapages, I don't think it will be used.

    List **buffers; // Array of list of buffers. One list of buffer per level.
 	TupleDesc tDesc;

    unsigned int level; // Current level being usd on the hierarchical trees
} *OSTRelation;

typedef struct OSTVBlock{
    int id;
    char* page;
} *OSTVBlock;

/*
 * RelationGetRelid
 *		Returns the OID of the relation
 */
#define RelationGetRelid_ost(relation) ((relation)->rd_id)

extern OSTRelation InitOSTRelation(OSTreeState relstate, unsigned int oid, char* attrDesc, unsigned int attrDescLength);

extern Buffer ReadBuffer_ost(OSTRelation relation, BlockNumber blockNum);

extern Page BufferGetPage_ost(OSTRelation relation, Buffer buffer);

void MarkBufferDirty_ost(OSTRelation relation, Buffer buffer);

extern void ReleaseBuffer_ost(OSTRelation relation, Buffer buffer);

extern BlockNumber BufferGetBlockNumber_ost(Buffer buffer);

extern void closeOSTRelation(OSTRelation rel);

//extern void setclevel(unsigned int nlevel);

#endif	/* SOE_OST_BUFMGR_H */