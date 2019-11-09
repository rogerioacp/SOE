#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_initSOE_t
{
	const char *ms_tName;
	size_t		ms_tName_len;
	const char *ms_iName;
	size_t		ms_iName_len;
	int			ms_tNBlocks;
	int			ms_nBlocks;
	unsigned int ms_tOid;
	unsigned int ms_iOid;
	unsigned int ms_functionOid;
	unsigned int ms_indexHandler;
	char	   *ms_pg_attr_desc;
	unsigned int ms_pgDescSize;
}			ms_initSOE_t;

typedef struct ms_insert_t
{
	const char *ms_heapTuple;
	unsigned int ms_tupleSize;
	char	   *ms_datum;
	unsigned int ms_datumSize;
}			ms_insert_t;

typedef struct ms_getTuple_t
{
	int			ms_retval;
	unsigned int ms_opmode;
	unsigned int ms_opoid;
	const char *ms_scanKey;
	int			ms_scanKeySize;
	char	   *ms_tuple;
	unsigned int ms_tupleLen;
	char	   *ms_tupleData;
	unsigned int ms_tupleDataLen;
}			ms_getTuple_t;

typedef struct ms_insertHeap_t
{
	const char *ms_heapTuple;
	unsigned int ms_tupleSize;
}			ms_insertHeap_t;

typedef struct ms_oc_logger_t
{
	const char *ms_str;
}			ms_oc_logger_t;

typedef struct ms_outFileInit_t
{
	const char *ms_filename;
	const char *ms_pages;
	unsigned int ms_nblocks;
	unsigned int ms_blocksize;
	int			ms_pagesSize;
}			ms_outFileInit_t;

typedef struct ms_outFileRead_t
{
	char	   *ms_page;
	const char *ms_filename;
	int			ms_blkno;
	int			ms_pageSize;
}			ms_outFileRead_t;

typedef struct ms_outFileWrite_t
{
	const char *ms_block;
	const char *ms_filename;
	int			ms_oblkno;
	int			ms_pageSize;
}			ms_outFileWrite_t;

typedef struct ms_outFileClose_t
{
	const char *ms_filename;
}			ms_outFileClose_t;

static sgx_status_t SGX_CDECL Enclave_oc_logger(void *pms)
{
	ms_oc_logger_t *ms = SGX_CAST(ms_oc_logger_t *, pms);

	oc_logger(ms->ms_str);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_outFileInit(void *pms)
{
	ms_outFileInit_t *ms = SGX_CAST(ms_outFileInit_t *, pms);

	outFileInit(ms->ms_filename, ms->ms_pages, ms->ms_nblocks, ms->ms_blocksize, ms->ms_pagesSize);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_outFileRead(void *pms)
{
	ms_outFileRead_t *ms = SGX_CAST(ms_outFileRead_t *, pms);

	outFileRead(ms->ms_page, ms->ms_filename, ms->ms_blkno, ms->ms_pageSize);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_outFileWrite(void *pms)
{
	ms_outFileWrite_t *ms = SGX_CAST(ms_outFileWrite_t *, pms);

	outFileWrite(ms->ms_block, ms->ms_filename, ms->ms_oblkno, ms->ms_pageSize);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_outFileClose(void *pms)
{
	ms_outFileClose_t *ms = SGX_CAST(ms_outFileClose_t *, pms);

	outFileClose(ms->ms_filename);

	return SGX_SUCCESS;
}

static const struct
{
	size_t		nr_ocall;
	void	   *table[5];
}			ocall_table_Enclave = {

	5,
	{
		(void *) Enclave_oc_logger,
		(void *) Enclave_outFileInit,
		(void *) Enclave_outFileRead,
		(void *) Enclave_outFileWrite,
		(void *) Enclave_outFileClose,
	}
};

sgx_status_t
initSOE(sgx_enclave_id_t eid, const char *tName, const char *iName, int tNBlocks, int nBlocks, unsigned int tOid, unsigned int iOid, unsigned int functionOid, unsigned int indexHandler, char *pg_attr_desc, unsigned int pgDescSize)
{
	sgx_status_t status;
	ms_initSOE_t ms;

	ms.ms_tName = tName;
	ms.ms_tName_len = tName ? strlen(tName) + 1 : 0;
	ms.ms_iName = iName;
	ms.ms_iName_len = iName ? strlen(iName) + 1 : 0;
	ms.ms_tNBlocks = tNBlocks;
	ms.ms_nBlocks = nBlocks;
	ms.ms_tOid = tOid;
	ms.ms_iOid = iOid;
	ms.ms_functionOid = functionOid;
	ms.ms_indexHandler = indexHandler;
	ms.ms_pg_attr_desc = pg_attr_desc;
	ms.ms_pgDescSize = pgDescSize;
	status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
	return status;
}

sgx_status_t
insert(sgx_enclave_id_t eid, const char *heapTuple, unsigned int tupleSize, char *datum, unsigned int datumSize)
{
	sgx_status_t status;
	ms_insert_t ms;

	ms.ms_heapTuple = heapTuple;
	ms.ms_tupleSize = tupleSize;
	ms.ms_datum = datum;
	ms.ms_datumSize = datumSize;
	status = sgx_ecall(eid, 1, &ocall_table_Enclave, &ms);
	return status;
}

sgx_status_t
getTuple(sgx_enclave_id_t eid, int *retval, unsigned int opmode, unsigned int opoid, const char *scanKey, int scanKeySize, char *tuple, unsigned int tupleLen, char *tupleData, unsigned int tupleDataLen)
{
	sgx_status_t status;
	ms_getTuple_t ms;

	ms.ms_opmode = opmode;
	ms.ms_opoid = opoid;
	ms.ms_scanKey = scanKey;
	ms.ms_scanKeySize = scanKeySize;
	ms.ms_tuple = tuple;
	ms.ms_tupleLen = tupleLen;
	ms.ms_tupleData = tupleData;
	ms.ms_tupleDataLen = tupleDataLen;
	status = sgx_ecall(eid, 2, &ocall_table_Enclave, &ms);
	if (status == SGX_SUCCESS && retval)
		*retval = ms.ms_retval;
	return status;
}

sgx_status_t
insertHeap(sgx_enclave_id_t eid, const char *heapTuple, unsigned int tupleSize)
{
	sgx_status_t status;
	ms_insertHeap_t ms;

	ms.ms_heapTuple = heapTuple;
	ms.ms_tupleSize = tupleSize;
	status = sgx_ecall(eid, 3, &ocall_table_Enclave, &ms);
	return status;
}
