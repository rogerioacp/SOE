
#include <oram/oram.h>
#include <collectc/list.h>


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
#define BufferGetPageSize(vbuffer, buffer) \
( \
	(Size)BLCKSZ \
)

#define BufferIsValid(vbuffer, bufnum) \
( \
	(bufnum) != InvalidBuffer  \
)



typedef struct VRelation{
	int lastBlock;
	ORAMState oram;
	List *buffer; //Buffer containing relation pages
	/* available for use by index AM. 
	 * Similar to a normal relation 
	*/
	void *rd_amcache;
} *VRelation;

typedef struct VBlock{
    int id;
    char* page;
} *VBlock;

VRelation initVRelation(ORAMState relstate);

Buffer ReadBuffer(VRelation relation, BlockNumber blockNum);

Page BufferGetPage(VRelation relation, Buffer buffer)

void MarkBufferDirty(VRelation *relation, Buffer buffer);