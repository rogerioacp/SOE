

#ifndef SOE_BUFMGR_H
#define SOE_BUFMGR_H



#include "storage/soe_buf.h"
#include "storage/soe_bufpage.h"
#include "storage/soe_block.h"

#include <oram/oram.h>
#include <oram/plblock.h>
#include <collectc/list.h>


/*
 * RelationGetTargetPageFreeSpace
 *		Returns the relation's desired freespace per page in bytes.
 */
#define RelationGetTargetPageFreeSpace_s(relation, defaultff) \
	(BLCKSZ * (100 - defaultff) / 100)

#define HEAP_DEFAULT_FILLFACTOR		10


/*

 * BufferGetPageSize
 *		Returns the page size within a buffer.
 *
 * Notes:
 *		Assumes buffer is valid.
 *
 *		The buffer can be a raw disk block and need not contain a valid
 *		(formatted) disk page.
 */
/* XXX should dig out of buffer descriptor */
#define BufferGetPageSize_s(vbuffer, buffer) \
( \
	(Size)BLCKSZ \
)

#define BufferIsValid_s(vbuffer, bufnum) \
( \
	(bufnum) != InvalidBuffer  \
)

/*
 * Buffer content lock modes (mode argument for LockBuffer())
 */
#define BUFFER_LOCK_UNLOCK		0
#define BUFFER_LOCK_SHARE		1
#define BUFFER_LOCK_EXCLUSIVE	2

typedef void (*pageinit_function) (Page page, BlockNumber blockNum, Size blocksize);

typedef struct VRelation{
	BlockNumber currentBlock;
	BlockNumber lastFreeBlock;
	unsigned int rd_id; // Original Relation Oid
	int totalBlocks;
	int* fsm; // in memory free space map that keeps the number of items in each block

	ORAMState oram;
	List *buffer; //Buffer containing relation pages
	/* available for use by index AM. 
	 * Similar to a normal relation 
	*/
	void *rd_amcache;
	pageinit_function pageinit;
} *VRelation;

typedef struct VBlock{
    int id;
    char* page;
} *VBlock;

/*
 * RelationGetRelid
 *		Returns the OID of the relation
 */
#define RelationGetRelid_s(relation) ((relation)->rd_id)


/* special block number for ReadBuffer() */
#define P_NEW	InvalidBlockNumber	/* grow the file to get a new page */


extern VRelation InitVRelation(ORAMState relstate, unsigned int oid,  int total_blocks, pageinit_function pg_f);

extern Buffer ReadBuffer_s(VRelation relation, BlockNumber blockNum);

extern Page BufferGetPage_s(VRelation relation, Buffer buffer);

extern void MarkBufferDirty_s(VRelation relation, Buffer buffer);

extern void ReleaseBuffer_s(VRelation relation, Buffer buffer);

extern BlockNumber BufferGetBlockNumber_s(Buffer buffer);

extern BlockNumber NumberOfBlocks_s(VRelation rel);

extern BlockNumber FreeSpaceBlock_s(VRelation rel);

extern void UpdateFSM(VRelation rel);

extern void BufferFull_s(VRelation rel, Buffer buffer);

extern void closeVRelation(VRelation rel);

#endif /* SOE_BUFPAGE_H */