/*-------------------------------------------------------------------------
 *
 * soe_pe.h
 *	  Implementation of page block encryption/decryption.
 * 
 *
 *
 * Copyright (c) 2018-2019, HASLab
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef SOE_PE_H
#define SOE_PE_H


void page_encryption(unsigned char* plaintextBlock, unsigned char* ciphertextBlock);
void page_decryption(unsigned char* ciphertextBlock, unsigned char* plaintextBlock);

#endif /* SOE_LOG_H */