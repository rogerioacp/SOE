
#ifndef ENCLAVE_DT_H
#define ENCLAVE_DT_H

#define SGX_SUCCESS 1

typedef unsigned int sgx_status_t;



void		initSOE(const char *tName, const char *iName, int tNBlocks, 
                    int* fanouts, unsigned int fanout_size,
                    unsigned int nlevels,int nBlocks, unsigned int tOid,
                    unsigned int iOid, unsigned int functionOid, 
                    unsigned int indexHandler, char *attrDesc, 
                    unsigned int attrDescLength);

void		initFSOE(const char *tName, const char *iName, int tNBlocks, 
                     int *fanout, unsigned int fanout_size, 
                     unsigned int nlevels, unsigned int tOid, 
                     unsigned int iOid, char *pg_attr_desc, 
                     unsigned int pgDescSize);

void		insert(const char *heapTuple, unsigned int tupleSize, 
                   char *datum, unsigned int datumSize);

void		addIndexBlock(char *block, unsigned int blockSize, 
                          unsigned int offset, unsigned int level);

void		addHeapBlock(char *block, unsigned int blockSize, 
                         unsigned int blkno);

void		insertHeap(const char *heapTuple, unsigned int tupleSize);

int			getTuple(unsigned int opmode, unsigned int opoid, 
                     const char *key, int scanKeySize, char *tuple, 
                     unsigned int tupleLen, char *tupleData, 
                     unsigned int tupleDataLen);

void		closeSoe();

extern void oc_logger(const char *str);
extern sgx_status_t outFileInit(const char *filename, const char *pages, 
                                unsigned int nblocks, unsigned int blocksize,
                                int pagesSize, int boffset);

extern sgx_status_t outFileRead(char *page, const char *filename, int blkno, 
                                int pageSize);
extern sgx_status_t outFileWrite(const char *block, const char *filename, 
                                 int oblkno, int pageSize);
extern sgx_status_t outFileClose(const char *filename);

#endif          /*ENCLAVE_DT_H*/
