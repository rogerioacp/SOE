#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)


typedef struct ms_initSOE_t {
	const char* ms_tName;
	size_t ms_tName_len;
	const char* ms_iName;
	size_t ms_iName_len;
	int ms_tNBlocks;
	int ms_nBlocks;
	unsigned int ms_tOid;
	unsigned int ms_iOid;
	unsigned int ms_functionOid;
	char* ms_pg_attr_desc;
	unsigned int ms_pgDescSize;
} ms_initSOE_t;

typedef struct ms_insert_t {
	const char* ms_heapTuple;
	unsigned int ms_tupleSize;
	const char* ms_datum;
	unsigned int ms_datumSize;
} ms_insert_t;

typedef struct ms_getTuple_t {
	int ms_retval;
	unsigned int ms_opmode;
	const char* ms_scanKey;
	int ms_scanKeySize;
	char* ms_tuple;
	unsigned int ms_tupleLen;
	char* ms_tupleData;
	unsigned int ms_tupleDataLen;
} ms_getTuple_t;

typedef struct ms_insertHeap_t {
	const char* ms_heapTuple;
	unsigned int ms_tupleSize;
} ms_insertHeap_t;

typedef struct ms_oc_logger_t {
	const char* ms_str;
} ms_oc_logger_t;

typedef struct ms_outFileInit_t {
	const char* ms_filename;
	const char* ms_pages;
	unsigned int ms_nblocks;
	unsigned int ms_blocksize;
	int ms_pagesSize;
} ms_outFileInit_t;

typedef struct ms_outFileRead_t {
	char* ms_page;
	const char* ms_filename;
	int ms_blkno;
	int ms_pageSize;
} ms_outFileRead_t;

typedef struct ms_outFileWrite_t {
	const char* ms_block;
	const char* ms_filename;
	int ms_oblkno;
	int ms_pageSize;
} ms_outFileWrite_t;

typedef struct ms_outFileClose_t {
	const char* ms_filename;
} ms_outFileClose_t;

static sgx_status_t SGX_CDECL sgx_initSOE(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_initSOE_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_initSOE_t* ms = SGX_CAST(ms_initSOE_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_tName = ms->ms_tName;
	size_t _len_tName = ms->ms_tName_len ;
	char* _in_tName = NULL;
	const char* _tmp_iName = ms->ms_iName;
	size_t _len_iName = ms->ms_iName_len ;
	char* _in_iName = NULL;
	char* _tmp_pg_attr_desc = ms->ms_pg_attr_desc;
	unsigned int _tmp_pgDescSize = ms->ms_pgDescSize;
	size_t _len_pg_attr_desc = _tmp_pgDescSize;
	char* _in_pg_attr_desc = NULL;

	CHECK_UNIQUE_POINTER(_tmp_tName, _len_tName);
	CHECK_UNIQUE_POINTER(_tmp_iName, _len_iName);
	CHECK_UNIQUE_POINTER(_tmp_pg_attr_desc, _len_pg_attr_desc);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_tName != NULL && _len_tName != 0) {
		_in_tName = (char*)malloc(_len_tName);
		if (_in_tName == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_tName, _len_tName, _tmp_tName, _len_tName)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

		_in_tName[_len_tName - 1] = '\0';
		if (_len_tName != strlen(_in_tName) + 1)
		{
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}
	if (_tmp_iName != NULL && _len_iName != 0) {
		_in_iName = (char*)malloc(_len_iName);
		if (_in_iName == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_iName, _len_iName, _tmp_iName, _len_iName)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

		_in_iName[_len_iName - 1] = '\0';
		if (_len_iName != strlen(_in_iName) + 1)
		{
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}
	}
	if (_tmp_pg_attr_desc != NULL && _len_pg_attr_desc != 0) {
		_in_pg_attr_desc = (char*)malloc(_len_pg_attr_desc);
		if (_in_pg_attr_desc == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_pg_attr_desc, _len_pg_attr_desc, _tmp_pg_attr_desc, _len_pg_attr_desc)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}

	initSOE((const char*)_in_tName, (const char*)_in_iName, ms->ms_tNBlocks, ms->ms_nBlocks, ms->ms_tOid, ms->ms_iOid, ms->ms_functionOid, _in_pg_attr_desc, _tmp_pgDescSize);
err:
	if (_in_tName) free(_in_tName);
	if (_in_iName) free(_in_iName);
	if (_in_pg_attr_desc) free(_in_pg_attr_desc);

	return status;
}

static sgx_status_t SGX_CDECL sgx_insert(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_insert_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_insert_t* ms = SGX_CAST(ms_insert_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_heapTuple = ms->ms_heapTuple;
	unsigned int _tmp_tupleSize = ms->ms_tupleSize;
	size_t _len_heapTuple = _tmp_tupleSize;
	char* _in_heapTuple = NULL;
	const char* _tmp_datum = ms->ms_datum;
	unsigned int _tmp_datumSize = ms->ms_datumSize;
	size_t _len_datum = _tmp_datumSize;
	char* _in_datum = NULL;

	CHECK_UNIQUE_POINTER(_tmp_heapTuple, _len_heapTuple);
	CHECK_UNIQUE_POINTER(_tmp_datum, _len_datum);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_heapTuple != NULL && _len_heapTuple != 0) {
		_in_heapTuple = (char*)malloc(_len_heapTuple);
		if (_in_heapTuple == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_heapTuple, _len_heapTuple, _tmp_heapTuple, _len_heapTuple)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}
	if (_tmp_datum != NULL && _len_datum != 0) {
		_in_datum = (char*)malloc(_len_datum);
		if (_in_datum == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_datum, _len_datum, _tmp_datum, _len_datum)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}

	insert((const char*)_in_heapTuple, _tmp_tupleSize, (const char*)_in_datum, _tmp_datumSize);
err:
	if (_in_heapTuple) free(_in_heapTuple);
	if (_in_datum) free(_in_datum);

	return status;
}

static sgx_status_t SGX_CDECL sgx_getTuple(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_getTuple_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_getTuple_t* ms = SGX_CAST(ms_getTuple_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_scanKey = ms->ms_scanKey;
	int _tmp_scanKeySize = ms->ms_scanKeySize;
	size_t _len_scanKey = _tmp_scanKeySize;
	char* _in_scanKey = NULL;
	char* _tmp_tuple = ms->ms_tuple;
	unsigned int _tmp_tupleLen = ms->ms_tupleLen;
	size_t _len_tuple = _tmp_tupleLen;
	char* _in_tuple = NULL;
	char* _tmp_tupleData = ms->ms_tupleData;
	unsigned int _tmp_tupleDataLen = ms->ms_tupleDataLen;
	size_t _len_tupleData = _tmp_tupleDataLen;
	char* _in_tupleData = NULL;

	CHECK_UNIQUE_POINTER(_tmp_scanKey, _len_scanKey);
	CHECK_UNIQUE_POINTER(_tmp_tuple, _len_tuple);
	CHECK_UNIQUE_POINTER(_tmp_tupleData, _len_tupleData);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_scanKey != NULL && _len_scanKey != 0) {
		_in_scanKey = (char*)malloc(_len_scanKey);
		if (_in_scanKey == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_scanKey, _len_scanKey, _tmp_scanKey, _len_scanKey)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}
	if (_tmp_tuple != NULL && _len_tuple != 0) {
		if ((_in_tuple = (char*)malloc(_len_tuple)) == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		memset((void*)_in_tuple, 0, _len_tuple);
	}
	if (_tmp_tupleData != NULL && _len_tupleData != 0) {
		if ((_in_tupleData = (char*)malloc(_len_tupleData)) == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		memset((void*)_in_tupleData, 0, _len_tupleData);
	}

	ms->ms_retval = getTuple(ms->ms_opmode, (const char*)_in_scanKey, _tmp_scanKeySize, _in_tuple, _tmp_tupleLen, _in_tupleData, _tmp_tupleDataLen);
err:
	if (_in_scanKey) free(_in_scanKey);
	if (_in_tuple) {
		if (memcpy_s(_tmp_tuple, _len_tuple, _in_tuple, _len_tuple)) {
			status = SGX_ERROR_UNEXPECTED;
		}
		free(_in_tuple);
	}
	if (_in_tupleData) {
		if (memcpy_s(_tmp_tupleData, _len_tupleData, _in_tupleData, _len_tupleData)) {
			status = SGX_ERROR_UNEXPECTED;
		}
		free(_in_tupleData);
	}

	return status;
}

static sgx_status_t SGX_CDECL sgx_insertHeap(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_insertHeap_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_insertHeap_t* ms = SGX_CAST(ms_insertHeap_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	const char* _tmp_heapTuple = ms->ms_heapTuple;
	unsigned int _tmp_tupleSize = ms->ms_tupleSize;
	size_t _len_heapTuple = _tmp_tupleSize;
	char* _in_heapTuple = NULL;

	CHECK_UNIQUE_POINTER(_tmp_heapTuple, _len_heapTuple);

	//
	// fence after pointer checks
	//
	sgx_lfence();

	if (_tmp_heapTuple != NULL && _len_heapTuple != 0) {
		_in_heapTuple = (char*)malloc(_len_heapTuple);
		if (_in_heapTuple == NULL) {
			status = SGX_ERROR_OUT_OF_MEMORY;
			goto err;
		}

		if (memcpy_s(_in_heapTuple, _len_heapTuple, _tmp_heapTuple, _len_heapTuple)) {
			status = SGX_ERROR_UNEXPECTED;
			goto err;
		}

	}

	insertHeap((const char*)_in_heapTuple, _tmp_tupleSize);
err:
	if (_in_heapTuple) free(_in_heapTuple);

	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv;} ecall_table[4];
} g_ecall_table = {
	4,
	{
		{(void*)(uintptr_t)sgx_initSOE, 0},
		{(void*)(uintptr_t)sgx_insert, 0},
		{(void*)(uintptr_t)sgx_getTuple, 0},
		{(void*)(uintptr_t)sgx_insertHeap, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[5][4];
} g_dyn_entry_table = {
	5,
	{
		{0, 0, 0, 0, },
		{0, 0, 0, 0, },
		{0, 0, 0, 0, },
		{0, 0, 0, 0, },
		{0, 0, 0, 0, },
	}
};


sgx_status_t SGX_CDECL oc_logger(const char* str)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = str ? strlen(str) + 1 : 0;

	ms_oc_logger_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_oc_logger_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(str, _len_str);

	ocalloc_size += (str != NULL) ? _len_str : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_oc_logger_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_oc_logger_t));
	ocalloc_size -= sizeof(ms_oc_logger_t);

	if (str != NULL) {
		ms->ms_str = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, str, _len_str)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_str);
		ocalloc_size -= _len_str;
	} else {
		ms->ms_str = NULL;
	}
	
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileInit(const char* filename, const char* pages, unsigned int nblocks, unsigned int blocksize, int pagesSize)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;
	size_t _len_pages = pagesSize;

	ms_outFileInit_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileInit_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(filename, _len_filename);
	CHECK_ENCLAVE_POINTER(pages, _len_pages);

	ocalloc_size += (filename != NULL) ? _len_filename : 0;
	ocalloc_size += (pages != NULL) ? _len_pages : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileInit_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileInit_t));
	ocalloc_size -= sizeof(ms_outFileInit_t);

	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	if (pages != NULL) {
		ms->ms_pages = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, pages, _len_pages)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_pages);
		ocalloc_size -= _len_pages;
	} else {
		ms->ms_pages = NULL;
	}
	
	ms->ms_nblocks = nblocks;
	ms->ms_blocksize = blocksize;
	ms->ms_pagesSize = pagesSize;
	status = sgx_ocall(1, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileRead(char* page, const char* filename, int blkno, int pageSize)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_page = pageSize;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileRead_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileRead_t);
	void *__tmp = NULL;

	void *__tmp_page = NULL;

	CHECK_ENCLAVE_POINTER(page, _len_page);
	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (page != NULL) ? _len_page : 0;
	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileRead_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileRead_t));
	ocalloc_size -= sizeof(ms_outFileRead_t);

	if (page != NULL) {
		ms->ms_page = (char*)__tmp;
		__tmp_page = __tmp;
		memset(__tmp_page, 0, _len_page);
		__tmp = (void *)((size_t)__tmp + _len_page);
		ocalloc_size -= _len_page;
	} else {
		ms->ms_page = NULL;
	}
	
	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	ms->ms_blkno = blkno;
	ms->ms_pageSize = pageSize;
	status = sgx_ocall(2, ms);

	if (status == SGX_SUCCESS) {
		if (page) {
			if (memcpy_s((void*)page, _len_page, __tmp_page, _len_page)) {
				sgx_ocfree();
				return SGX_ERROR_UNEXPECTED;
			}
		}
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileWrite(const char* block, const char* filename, int oblkno, int pageSize)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_block = pageSize;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileWrite_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileWrite_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(block, _len_block);
	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (block != NULL) ? _len_block : 0;
	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileWrite_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileWrite_t));
	ocalloc_size -= sizeof(ms_outFileWrite_t);

	if (block != NULL) {
		ms->ms_block = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, block, _len_block)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_block);
		ocalloc_size -= _len_block;
	} else {
		ms->ms_block = NULL;
	}
	
	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	ms->ms_oblkno = oblkno;
	ms->ms_pageSize = pageSize;
	status = sgx_ocall(3, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL outFileClose(const char* filename)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_filename = filename ? strlen(filename) + 1 : 0;

	ms_outFileClose_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_outFileClose_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(filename, _len_filename);

	ocalloc_size += (filename != NULL) ? _len_filename : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_outFileClose_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_outFileClose_t));
	ocalloc_size -= sizeof(ms_outFileClose_t);

	if (filename != NULL) {
		ms->ms_filename = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, filename, _len_filename)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_filename);
		ocalloc_size -= _len_filename;
	} else {
		ms->ms_filename = NULL;
	}
	
	status = sgx_ocall(4, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

